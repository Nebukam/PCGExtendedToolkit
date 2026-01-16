// Copyright 2026 Timothé Lapetite and contributors
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
		// Build the DCEL and enumerate all faces
		FPlanarFaceEnumerator Enumerator;
		Enumerator.Build(InCluster, ProjectedPositions);

		// Create minimal constraints for wrapper detection - no filtering
		TSharedPtr<FCellConstraints> TempConstraints = MakeShared<FCellConstraints>();
		TempConstraints->bKeepCellsWithLeaves = true;
		TempConstraints->bDuplicateLeafPoints = false;

		// Enumerate all faces
		TArray<TSharedPtr<FCell>> AllCells;
		Enumerator.EnumerateAllFaces(AllCells, TempConstraints.ToSharedRef());

		// The wrapper/exterior face is simply the one with the largest area
		// In a proper planar graph, the exterior face encloses all others
		double LargestArea = -MAX_dbl;
		for (const TSharedPtr<FCell>& Cell : AllCells)
		{
			if (Cell && Cell->Data.Area > LargestArea)
			{
				LargestArea = Cell->Data.Area;
				WrapperCell = Cell;
			}
		}

		if (WrapperCell)
		{
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

	ECellResult FCell::BuildFromCluster(const PCGExGraphs::FLink InSeedLink, TSharedRef<FCluster> InCluster, const TArray<FVector2D>& ProjectedPositions)
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
		PCGExGraphs::FLink From = InSeedLink;
		// To node, through edge
		PCGExGraphs::FLink To = PCGExGraphs::FLink(InCluster->GetEdgeOtherNode(From)->Index, Seed.Edge);

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
			if (SignedEdges.Num() > FailSafe) { return ECellResult::MalformedCluster; }

			bool bEdgeAlreadyExists;
			uint64 SignedEdgeHash = PCGEx::H64(From.Node, To.Node);

			SignedEdges.Add(SignedEdgeHash, &bEdgeAlreadyExists);
			if (bEdgeAlreadyExists)
			{
				if (To.Edge == Seed.Edge || To.Node == Seed.Node)
				{
					Data.bIsClosedLoop = true;
					const int32 RemovedIndex = Nodes.Pop();
					if (RemovedIndex == Nodes.Last()) { Nodes.Pop(); }
					break;
				}

				return ECellResult::OpenCell;
			}

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

			// Seek next best candidate using DCEL-style angle calculation
			// At current vertex, find the outgoing edge that comes just AFTER the incoming edge's reverse in CCW order
			const FVector2D CurrentPos = ProjectedPositions[Current->PointIndex];
			const FVector2D FromPos = ProjectedPositions[InCluster->GetNodePointIndex(From.Node)];

			// The incoming edge's reverse direction: from current back to where we came from
			const FVector2D IncomingReverse = (FromPos - CurrentPos).GetSafeNormal();
			const double IncomingReverseAngle = FMath::Atan2(IncomingReverse.Y, IncomingReverse.X);

			From = To;
			To = PCGExGraphs::FLink(-1, -1);

			// Find the outgoing edge with smallest CCW angle from the incoming reverse
			double SmallestCCWAngle = MAX_dbl;
			for (const PCGExGraphs::FLink& Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (Lk.Edge == LockedEdge) { continue; }

				const FVector2D NeighborPos = ProjectedPositions[InCluster->GetNodePointIndex(NeighborIndex)];
				const FVector2D OutgoingDir = (NeighborPos - CurrentPos).GetSafeNormal();
				const double OutgoingAngle = FMath::Atan2(OutgoingDir.Y, OutgoingDir.X);

				// Compute CCW angle from incoming reverse to this outgoing edge
				// This gives us the angle we'd turn CCW from where we came from
				double CCWAngle = OutgoingAngle - IncomingReverseAngle;
				if (CCWAngle <= 0) { CCWAngle += TWO_PI; }  // Wrap to (0, 2π]

				if (CCWAngle < SmallestCCWAngle)
				{
					SmallestCCWAngle = CCWAngle;
					To = Lk;
				}
			}

			if (To.Node == -1) { return ECellResult::OpenCell; }

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

		PCGExArrayHelpers::ShiftArrayToSmallest(Nodes);

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

		Data.Area *= 0.01;
		if (Constraints->MinArea > Data.Area || Data.Area > Constraints->MaxArea) { return ECellResult::OutsideAreaLimit; }

		if (Constraints->WrapperCell && Constraints->WrapperClassificationTolerance > 0 && FMath::IsNearlyEqual(Data.Area, Constraints->WrapperCell->Data.Area, Constraints->WrapperClassificationTolerance))
		{
			return ECellResult::WrapperCell;
		}

		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromCluster(const FVector& SeedPosition, const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FPCGExNodeSelectionDetails* Picking)
	{
		// Project the seed position to 2D
		const FVector ProjectedSeed3D = ProjectionDetails.ProjectFlat(SeedPosition);
		const FVector2D Seed2D(ProjectedSeed3D.X, ProjectedSeed3D.Y);

		// Find the globally closest edge in 3D world space using EdgeOctree
		double BestDistSq = MAX_dbl;
		int32 BestEdgeIdx = -1;

		InCluster->GetEdgeOctree()->FindNearbyElements(SeedPosition, [&](const PCGExOctree::FItem& Item)
		{
			const double Dist = InCluster->GetPointDistToEdgeSquared(Item.Index, SeedPosition);
			if (Dist < BestDistSq)
			{
				BestDistSq = Dist;
				BestEdgeIdx = Item.Index;
			}
		});

		if (BestEdgeIdx == -1)
		{
			return ECellResult::Unknown;
		}

		// Optional: Apply picking distance constraint
		if (Picking)
		{
			const FEdge* Edge = InCluster->GetEdge(BestEdgeIdx);
			const FVector EdgeStart3D = InCluster->GetStartPos(Edge);
			const FVector EdgeEnd3D = InCluster->GetEndPos(Edge);
			const FVector ClosestOnEdge = FMath::ClosestPointOnSegment(SeedPosition, EdgeStart3D, EdgeEnd3D);
			if (!Picking->WithinDistance(ClosestOnEdge, SeedPosition))
			{
				return ECellResult::Unknown;
			}
		}

		// Get edge endpoints
		const FEdge* Edge = InCluster->GetEdge(BestEdgeIdx);
		const int32 StartPointIdx = Edge->Start;
		const int32 EndPointIdx = Edge->End;
		const int32 StartNodeIdx = InCluster->NodeIndexLookup->Get(StartPointIdx);
		const int32 EndNodeIdx = InCluster->NodeIndexLookup->Get(EndPointIdx);

		// Determine which side of the edge the seed is on using 2D perp dot
		const FVector2D& EdgeStart2D = ProjectedPositions[StartPointIdx];
		const FVector2D& EdgeEnd2D = ProjectedPositions[EndPointIdx];
		const FVector2D EdgeDir = EdgeEnd2D - EdgeStart2D;
		const FVector2D SeedDir = Seed2D - EdgeStart2D;
		const double PerpDot = EdgeDir.X * SeedDir.Y - EdgeDir.Y * SeedDir.X;

		// Choose starting node based on which side the seed is on
		// PerpDot > 0 means seed is on LEFT of edge (Start→End direction)
		// We want to trace the face that's on the SAME side as the seed
		PCGExGraphs::FLink Link;
		Link.Edge = BestEdgeIdx;
		Link.Node = (PerpDot >= 0) ? EndNodeIdx : StartNodeIdx;

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
