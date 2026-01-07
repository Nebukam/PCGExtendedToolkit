// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExCell.h"
#include "Misc/ScopeExit.h"

#include "Clusters/Artifacts/PCGExCellDetails.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Math/PCGExMath.h"
#include "Math/PCGExMathAxis.h"
#include "Math/Geo/PCGExGeo.h"
#include "Paths/PCGExPathsCommon.h"

namespace PCGExClusters
{
	void SetPointProperty(PCGExData::FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty)
	{
		if (InProperty == EPCGExPointPropertyOutput::Density)
		{
			TPCGValueRange<float> Density = InPoint.Data->GetDensityValueRange(false);
			Density[InPoint.Index] = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::Steepness)
		{
			TPCGValueRange<float> Steepness = InPoint.Data->GetSteepnessValueRange(false);
			Steepness[InPoint.Index] = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorR)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(0) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorG)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(1) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorB)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(2) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorA)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(3) = InValue;
		}
	}

	bool FHoles::Overlaps(const TArray<FVector2D>& Polygon)
	{
		{
			FReadScopeLock ReadScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return PCGExMath::Geo::IsAnyPointInPolygon(ProjectedPoints, Polygon); }
		}

		{
			FWriteScopeLock WriteScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return PCGExMath::Geo::IsAnyPointInPolygon(ProjectedPoints, Polygon); }

			ProjectionDetails.ProjectFlat(PointDataFacade, ProjectedPoints);
			return PCGExMath::Geo::IsAnyPointInPolygon(ProjectedPoints, Polygon);
		}
	}

	FCellConstraints::FCellConstraints(const FPCGExCellConstraintsDetails& InDetails)
	{
		Winding = InDetails.OutputWinding;
		bConcaveOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConcaveOnly;
		bConvexOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConvexOnly;
		bKeepCellsWithLeaves = InDetails.bKeepCellsWithLeaves;
		bDuplicateLeafPoints = InDetails.bDuplicateLeafPoints;

		WrapperClassificationTolerance = InDetails.WrapperClassificationTolerance;
		bBuildWrapper = InDetails.bOmitWrappingBounds;

		if (InDetails.bOmitBelowPointCount) { MinPointCount = InDetails.MinPointCount; }
		if (InDetails.bOmitAbovePointCount) { MaxPointCount = InDetails.MaxPointCount; }

		if (InDetails.bOmitBelowBoundsSize) { MinBoundsSize = InDetails.MinBoundsSize; }
		if (InDetails.bOmitAboveBoundsSize) { MaxBoundsSize = InDetails.MaxBoundsSize; }

		if (InDetails.bOmitBelowArea) { MinArea = InDetails.MinArea; }
		if (InDetails.bOmitAboveArea) { MaxArea = InDetails.MaxArea; }

		if (InDetails.bOmitBelowPerimeter) { MinPerimeter = InDetails.MinPerimeter; }
		if (InDetails.bOmitAbovePerimeter) { MaxPerimeter = InDetails.MaxPerimeter; }

		if (InDetails.bOmitBelowSegmentLength) { MinSegmentLength = InDetails.MinSegmentLength; }
		if (InDetails.bOmitAboveSegmentLength) { MaxSegmentLength = InDetails.MaxSegmentLength; }

		if (InDetails.bOmitBelowCompactness) { MinCompactness = InDetails.MinCompactness; }
		if (InDetails.bOmitAboveCompactness) { MaxCompactness = InDetails.MaxCompactness; }
	}

	void FCellConstraints::Reserve(const int32 InCellHashReserve)
	{
		UniqueStartHalfEdgesHash.Reserve(InCellHashReserve);
		UniquePathsHashSet.Reserve(InCellHashReserve);
	}

	bool FCellConstraints::ContainsSignedEdgeHash(const uint64 Hash)
	{
		return UniqueStartHalfEdgesHash.Contains(Hash);
	}

	bool FCellConstraints::IsUniqueStartHalfEdge(const uint64 Hash)
	{
		bool bAlreadyExists = false;
		UniqueStartHalfEdgesHash.Add(Hash, bAlreadyExists);
		return !bAlreadyExists;
	}

	bool FCellConstraints::IsUniqueCellHash(const TSharedPtr<FCell>& InCell)
	{
		bool bAlreadyExists;
		UniquePathsHashSet.Add(InCell->GetCellHash(), bAlreadyExists);
		return !bAlreadyExists;
	}

	void FCellConstraints::BuildWrapperCell(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const TSharedPtr<FCellConstraints>& InConstraints)
	{
		double MaxDist = 0;
		FLink Link = FLink(-1, -1);
		const TArray<FNode>& Nodes = *InCluster->Nodes.Get();
		for (const FNode& Node : Nodes)
		{
			if (const double Dist = FVector2D::DistSquared(InCluster->ProjectedCentroid, ProjectedPositions[Node.PointIndex]); Dist > MaxDist)
			{
				Link.Node = Node.Index;
				MaxDist = Dist;
			}
		}

		if (Link.Node == -1)
		{
			WrapperCell = nullptr;
			return;
		}

		TSharedPtr<FCellConstraints> TempConstraints = InConstraints;
		if (!InConstraints)
		{
			TempConstraints = MakeShared<FCellConstraints>();
			TempConstraints->bKeepCellsWithLeaves = bKeepCellsWithLeaves;
			TempConstraints->bDuplicateLeafPoints = bDuplicateLeafPoints;
			TempConstraints->Winding = Winding;
		}

		// Find edge that's pointing away from the local center the most
		double BestDot = MAX_dbl;
		FNode* SeedNode = InCluster->GetNode(Link);
		const FVector2D From = ProjectedPositions[SeedNode->PointIndex];
		const FVector2D TowardCenter = (InCluster->ProjectedCentroid - From).GetSafeNormal();

		for (const FLink& Lk : SeedNode->Links)
		{
			double Dot = FVector2D::DotProduct(TowardCenter, (ProjectedPositions[InCluster->GetNodePointIndex(Lk)] - From).GetSafeNormal());
			if (Dot < BestDot)
			{
				BestDot = Dot;
				Link.Edge = Lk.Edge;
			}
		}

		if (Link.Edge == -1)
		{
			WrapperCell = nullptr;
			return;
		}

		auto GetGuidedHalfEdge = [&]()
		{
			FNode* StartNode = InCluster->GetEdgeStart(Link.Edge);
			FNode* EndNode = InCluster->GetEdgeEnd(Link.Edge);

			if (StartNode->IsLeaf() && !EndNode->IsLeaf()) { return StartNode; }
			if (EndNode->IsLeaf() && !StartNode->IsLeaf()) { return EndNode; }

			FVector2D EdgeDir = (ProjectedPositions[EndNode->PointIndex] - ProjectedPositions[StartNode->PointIndex]).GetSafeNormal();
			FVector2D Normal(-EdgeDir.Y, EdgeDir.X); // CCW normal

			if (FVector2D::DotProduct((ProjectedPositions[InCluster->GetNodePointIndex(Link)] - InCluster->ProjectedCentroid).GetSafeNormal(), Normal) > 0) { return StartNode; }
			return EndNode;
		};

		// Determine which node we should start with to be right-handed
		Link.Node = GetGuidedHalfEdge()->Index;

		if (const TSharedPtr<FCell> Cell = MakeShared<FCell>(TempConstraints.ToSharedRef()); Cell->BuildFromCluster(Link, InCluster, ProjectedPositions) == ECellResult::Success)
		{
			WrapperCell = Cell;
			IsUniqueCellHash(WrapperCell);
		}
	}

	void FCellConstraints::Cleanup()
	{
		WrapperCell = nullptr;
	}

	uint64 FCell::GetCellHash()
	{
		if (CellHash != 0) { return CellHash; }
		CellHash = CityHash64(reinterpret_cast<const char*>(Nodes.GetData()), Nodes.Num() * sizeof(int32));
		return CellHash;
	}

	ECellResult FCell::BuildFromCluster(const FLink InSeedLink, TSharedRef<FCluster> InCluster, const TArray<FVector2D>& ProjectedPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCell::BuildFromCluster);

		ON_SCOPE_EXIT
		{
			Nodes.Shrink();
		};

		bBuiltSuccessfully = false;
		Data.Bounds = FBox(ForceInit);

		Seed = InSeedLink;
		// From node, through edge; edge will be updated to be last traversed after.
		FLink From = InSeedLink;
		// To node, through edge
		FLink To = FLink(InCluster->GetEdgeOtherNode(From)->Index, Seed.Edge);

		const uint64 SeedHalfEdge = PCGEx::H64(From.Node, To.Node);
		if (!Constraints->IsUniqueStartHalfEdge(SeedHalfEdge)) { return ECellResult::Duplicate; }

		const FVector SeedRP = InCluster->GetPos(From.Node);

		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(SeedRP);
		Data.Centroid = SeedRP;
		Data.Bounds += SeedRP;

		Nodes.Reserve(32);
		Nodes.Add(From.Node);
		if (InCluster->GetNode(From.Node)->IsLeaf() && Constraints->bDuplicateLeafPoints) { Nodes.Add(From.Node); }

		int32 NumUniqueNodes = 1;

		const int32 FailSafe = InCluster->Edges->Num() * 2;

		TSet<uint64> SignedEdges;
		SignedEdges.Reserve(32);

		while (To.Node != -1)
		{
			if (SignedEdges.Num() > FailSafe) { return ECellResult::MalformedCluster; } // Let's hope this never happens

			bool bEdgeAlreadyExists;
			uint64 SignedEdgeHash = PCGEx::H64(From.Node, To.Node);

			//if (SignedEdgeHash != UniqueStartEdgeHash && Constraints->ContainsSignedEdgeHash(SignedEdgeHash)) { return ECellResult::Duplicate; }

			SignedEdges.Add(SignedEdgeHash, &bEdgeAlreadyExists);
			if (bEdgeAlreadyExists)
			{
				if (To.Edge == Seed.Edge || To.Node == Seed.Node)
				{
					Data.bIsClosedLoop = true;
					const int32 RemovedIndex = Nodes.Pop();            // Remove last added point
					if (RemovedIndex == Nodes.Last()) { Nodes.Pop(); } // Remove last if duplicate (leaf)
					break;
				}

				return ECellResult::OpenCell;
			}

			// Add next node since it's valid

			const FNode* Current = InCluster->GetNode(To.Node);

			Nodes.Add(Current->Index);
			NumUniqueNodes++;

			const FVector& RP = InCluster->GetPos(Current);
			Data.Centroid += RP;

			double SegmentLength = 0;
			const double NewLength = Metrics.Add(RP, SegmentLength);
			if (NewLength > Constraints->MaxPerimeter) { return ECellResult::OutsidePerimeterLimit; }
			if (SegmentLength < Constraints->MinSegmentLength || SegmentLength > Constraints->MaxSegmentLength) { return ECellResult::OutsideSegmentsLimit; }

			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::OutsidePointsLimit; }

			Data.Bounds += RP;
			if (Data.Bounds.GetSize().Length() > Constraints->MaxBoundsSize) { return ECellResult::OutsideBoundsLimit; }

			int32 LockedEdge = To.Edge;

			if (Current->IsLeaf())
			{
				LockedEdge = -1;
				if (Constraints->bDuplicateLeafPoints) { Nodes.Add(Current->Index); }
			}

			// Seek next best candidate
			const FVector2D PP = ProjectedPositions[Current->PointIndex];
			const FVector2D GuideDir = (PP - ProjectedPositions[InCluster->GetNodePointIndex(From.Node)]).GetSafeNormal();

			From = To;
			To = FLink(-1, -1);

			double BestAngle = MAX_dbl;
			for (const FLink Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (Lk.Edge == LockedEdge) { continue; }

				const FVector2D OtherDir = (PP - ProjectedPositions[InCluster->GetNodePointIndex(NeighborIndex)]).GetSafeNormal();

				if (const double Angle = PCGExMath::GetRadiansBetweenVectors(OtherDir, GuideDir); Angle < BestAngle)
				{
					BestAngle = Angle;
					To = Lk;
				}
			}

			if (To.Node == -1) { return ECellResult::OpenCell; } // Failed to wrap

			if (InCluster->GetNode(To.Node)->Num() == 1 && !Constraints->bKeepCellsWithLeaves) { return ECellResult::Leaf; }
			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::OutsideBoundsLimit; }

			if (NumUniqueNodes > 2)
			{
				PCGExMath::CheckConvex(InCluster->GetPos(Nodes.Last(2)), InCluster->GetPos(Nodes.Last(1)), InCluster->GetPos(Nodes.Last()), Data.bIsConvex, Sign);

				if (Constraints->bConvexOnly && !Data.bIsConvex) { return ECellResult::WrongAspect; }
			}
		}

		if (NumUniqueNodes <= 2) { return ECellResult::Leaf; }

		if (!Data.bIsClosedLoop) { return ECellResult::OpenCell; }

		PCGExArrayHelpers::ShiftArrayToSmallest(Nodes); // ! important to guarantee contour determinism

		if (!Constraints->IsUniqueCellHash(SharedThis(this))) { return ECellResult::Duplicate; }

		bBuiltSuccessfully = true;

		Data.Centroid /= NumUniqueNodes;

		Data.Perimeter = Metrics.Length;
		const double LastSegmentLength = FVector::Dist(InCluster->GetPos(Nodes[0]), InCluster->GetPos(Nodes.Last()));
		if (Constraints->MinSegmentLength > LastSegmentLength || LastSegmentLength > Constraints->MaxSegmentLength) { return ECellResult::OutsideSegmentsLimit; }

		if (Data.Perimeter < Constraints->MinPerimeter || Data.Perimeter > Constraints->MaxPerimeter) { return ECellResult::OutsidePerimeterLimit; }

		if (Constraints->bConcaveOnly && Data.bIsConvex) { return ECellResult::WrongAspect; }
		if (NumUniqueNodes < Constraints->MinPointCount) { return ECellResult::OutsidePointsLimit; }
		if (Data.Bounds.GetSize().Length() < Constraints->MinBoundsSize) { return ECellResult::OutsideBoundsLimit; }

		Polygon.SetNumUninitialized(Nodes.Num());
		for (int i = 0; i < Nodes.Num(); ++i) { Polygon[i] = FVector2D(ProjectedPositions[InCluster->GetNodePointIndex(Nodes[i])]); }

		PCGExMath::FPolygonInfos PolyInfos = PCGExMath::FPolygonInfos(Polygon);

		Data.Area = PolyInfos.Area;
		Data.bIsClockwise = PolyInfos.bIsClockwise;
		Data.Compactness = PolyInfos.Compactness;

		if (!PolyInfos.IsWinded(Constraints->Winding))
		{
			Algo::Reverse(Nodes);
			Algo::Reverse(Polygon);
		}

		if (Constraints->Holes && Constraints->Holes->Overlaps(Polygon)) { return ECellResult::Hole; }
		if (Constraints->MinCompactness > Data.Compactness || Data.Compactness > Constraints->MaxCompactness) { return ECellResult::OutsideCompactnessLimit; }

		Data.Area *= 0.01; // QoL to avoid extra 000 in the detail panel.
		if (Constraints->MinArea > Data.Area || Data.Area > Constraints->MaxArea) { return ECellResult::OutsideAreaLimit; }

		if (Constraints->WrapperCell && Constraints->WrapperClassificationTolerance > 0 && FMath::IsNearlyEqual(Data.Area, Constraints->WrapperCell->Data.Area, Constraints->WrapperClassificationTolerance))
		{
			return ECellResult::WrapperCell;
		}

		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromCluster(const FVector& SeedPosition, const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const FVector& UpVector, const FPCGExNodeSelectionDetails* Picking)
	{
		FLink Link = FLink(-1, -1);
		Link.Node = InCluster->FindClosestNode(SeedPosition, Picking ? Picking->PickingMethod : EPCGExClusterClosestSearchMode::Edge, 2);

		if (Link.Node == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		if (const FVector StartPosition = InCluster->GetPos(Link.Node); Picking && !Picking->WithinDistance(StartPosition, SeedPosition))
		{
			// Fail. Not within radius.
			return ECellResult::Unknown;
		}

		// Find edge closest to seed position
		Link.Edge = InCluster->FindClosestEdge(Link.Node, SeedPosition, 2);

		if (Link.Edge == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		const FEdge* E = InCluster->GetEdge(Link.Edge);

		// choose a deterministic right-hand frame		
		Link.Node = InCluster->GetGuidedHalfEdge(Link.Edge, SeedPosition, UpVector)->Index;
		return BuildFromCluster(Link, InCluster, ProjectedPositions);
	}

	ECellResult FCell::BuildFromPath(const TArray<FVector2D>& ProjectedPositions)
	{
		return ECellResult::Unknown;
	}

	void FCell::PostProcessPoints(UPCGBasePointData* InMutablePoints)
	{
	}
}
