// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopology.h"

#include "PCGExCompare.h"
#include "Curve/CurveUtil.h"

void FPCGExCellSeedMutationDetails::ApplyToPoint(const PCGExTopology::FCell* InCell, FPCGPoint& OutPoint, const TArray<FPCGPoint>& CellPoints) const
{
	switch (Location)
	{
	default:
	case EPCGExCellSeedLocation::Original:
		break;
	case EPCGExCellSeedLocation::Centroid:
		OutPoint.Transform.SetLocation(InCell->Centroid);
		break;
	case EPCGExCellSeedLocation::PathBoundsCenter:
		OutPoint.Transform.SetLocation(InCell->Bounds.GetCenter());
		break;
	case EPCGExCellSeedLocation::FirstNode:
		OutPoint.Transform.SetLocation(CellPoints[0].Transform.GetLocation());
		break;
	case EPCGExCellSeedLocation::LastNode:
		OutPoint.Transform.SetLocation(CellPoints.Last().Transform.GetLocation());
		break;
	}

	if (bResetScale) { OutPoint.Transform.SetScale3D(FVector::OneVector); }

	if (bResetRotation) { OutPoint.Transform.SetRotation(FQuat::Identity); }

	if (bMatchCellBounds)
	{
		FVector Offset = OutPoint.Transform.GetLocation();
		OutPoint.BoundsMin = InCell->Bounds.Min - Offset;
		OutPoint.BoundsMax = InCell->Bounds.Max - Offset;
	}

	PCGExHelpers::SetPointProperty(OutPoint, InCell->Area, AreaTo);
	PCGExHelpers::SetPointProperty(OutPoint, InCell->Perimeter, PerimeterTo);
	PCGExHelpers::SetPointProperty(OutPoint, InCell->Compactness, CompactnessTo);
}

namespace PCGExTopology
{
	bool FHoles::Overlaps(const FGeometryScriptSimplePolygon& Polygon)
	{
		{
			FReadScopeLock ReadScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return IsAnyPointInPolygon(ProjectedPoints, Polygon); }
		}

		{
			FWriteScopeLock WriteScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return IsAnyPointInPolygon(ProjectedPoints, Polygon); }

			ProjectionDetails.ProjectFlat(PointDataFacade, ProjectedPoints);
			return IsAnyPointInPolygon(ProjectedPoints, Polygon);
		}
	}

	bool FCellConstraints::ContainsSignedEdgeHash(const uint64 Hash) const
	{
		FReadScopeLock ReadScopeLock(UniqueStartHalfEdgesHashLock);
		return UniqueStartHalfEdgesHash.Contains(Hash);
	}

	bool FCellConstraints::IsUniqueStartHalfEdge(const uint64 Hash)
	{
		bool bAlreadyExists = false;
		{
			FReadScopeLock ReadScopeLock(UniqueStartHalfEdgesHashLock);
			bAlreadyExists = UniqueStartHalfEdgesHash.Contains(Hash);
			if (bAlreadyExists) { return false; }
		}
		{
			FWriteScopeLock WriteScopeLock(UniqueStartHalfEdgesHashLock);
			UniqueStartHalfEdgesHash.Add(Hash, &bAlreadyExists);
			return !bAlreadyExists;
		}
	}

	bool FCellConstraints::IsUniqueCellHash(const TSharedPtr<FCell>& InCell)
	{
		const uint32 CellHash = InCell->GetCellHash();

		{
			FReadScopeLock ReadScopeLock(UniqueStartHalfEdgesHashLock);
			if (UniquePathsHashSet.Contains(CellHash)) { return false; }
		}

		{
			FWriteScopeLock WriteScope(UniqueStartHalfEdgesHashLock);
			bool bAlreadyExists;
			UniquePathsHashSet.Add(CellHash, &bAlreadyExists);
			return !bAlreadyExists;
		}
	}

	void FCellConstraints::BuildWrapperCell(TSharedRef<PCGExCluster::FCluster> InCluster, const TArray<FVector>& ProjectedPositions)
	{
		const FVector SeedWP = InCluster->Bounds.GetCenter() + InCluster->Bounds.GetSize() * FVector(1, 1, 0);

		TSharedPtr<FCellConstraints> TempConstraints = MakeShared<FCellConstraints>();
		TempConstraints->bKeepCellsWithLeaves = bKeepCellsWithLeaves;
		TempConstraints->bDuplicateLeafPoints = bDuplicateLeafPoints;

		WrapperCell = MakeShared<PCGExTopology::FCell>(TempConstraints.ToSharedRef());
		if (WrapperCell->BuildFromCluster(SeedWP, InCluster, ProjectedPositions) != ECellResult::Success) { WrapperCell = nullptr; }
		else { IsUniqueCellHash(WrapperCell); }
	}

	void FCellConstraints::Cleanup()
	{
		WrapperCell = nullptr;
	}

	uint32 FCell::GetCellHash()
	{
		if (CellHash != 0) { return CellHash; }

		TArray<int32> SortedNodes = Nodes;
		SortedNodes.Sort();

		for (int32 i = 0; i < SortedNodes.Num(); i++) { CellHash = HashCombineFast(CellHash, SortedNodes[i]); }

		return CellHash;
	}

	ECellResult FCell::BuildFromCluster(
		const PCGExGraph::FLink InSeedLink,
		TSharedRef<PCGExCluster::FCluster> InCluster,
		const TArray<FVector>& ProjectedPositions)
	{
		bBuiltSuccessfully = false;
		Bounds = FBox(ForceInit);

		Seed = InSeedLink;
		PCGExGraph::FLink From = InSeedLink;                                                           // From node, through edge; edge will be updated to be last traversed after.
		PCGExGraph::FLink To = PCGExGraph::FLink(InCluster->GetEdgeOtherNode(From)->Index, Seed.Edge); // To node, through edge

		const uint64 SeedHalfEdge = PCGEx::H64(From.Node, To.Node);
		if (!Constraints->IsUniqueStartHalfEdge(SeedHalfEdge)) { return ECellResult::Duplicate; }

		const FVector SeedRP = InCluster->GetPos(From.Node);

		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(SeedRP);
		Centroid = SeedRP;
		Bounds += SeedRP;

		Nodes.Add(From.Node);
		if (InCluster->GetNode(From.Node)->IsLeaf() && Constraints->bDuplicateLeafPoints) { Nodes.Add(From.Node); }

		int32 NumUniqueNodes = 1;

		const int32 FailSafe = InCluster->Edges->Num() * 2;
		TSet<uint64> SignedEdges;
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
					bIsClosedLoop = true;
					const int32 RemovedIndex = Nodes.Pop();            // Remove last added point
					if (RemovedIndex == Nodes.Last()) { Nodes.Pop(); } // Remove last if duplicate (leaf)
					break;
				}

				return ECellResult::OpenCell;
			}

			// Add next node since it's valid

			const PCGExCluster::FNode* Current = InCluster->GetNode(To.Node);

			Nodes.Add(Current->Index);
			NumUniqueNodes++;

			const FVector& RP = InCluster->GetPos(Current);
			Centroid += RP;

			double SegmentLength = 0;
			const double NewLength = Metrics.Add(RP, SegmentLength);
			if (NewLength > Constraints->MaxPerimeter) { return ECellResult::OutsidePerimeterLimit; }
			if (SegmentLength < Constraints->MinSegmentLength || SegmentLength > Constraints->MaxSegmentLength) { return ECellResult::OutsideSegmentsLimit; }

			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::OutsidePointsLimit; }

			Bounds += RP;
			if (Bounds.GetSize().Length() > Constraints->MaxBoundsSize) { return ECellResult::OutsideBoundsLimit; }

			int32 LockedEdge = To.Edge;

			if (Current->IsLeaf())
			{
				LockedEdge = -1;
				if (Constraints->bDuplicateLeafPoints) { Nodes.Add(Current->Index); }
			}

			// Seek next best candidate
			const FVector PP = ProjectedPositions[Current->PointIndex];
			const FVector GuideDir = (PP - ProjectedPositions[InCluster->GetNode(From.Node)->PointIndex]).GetSafeNormal();

			From = To;
			To = PCGExGraph::FLink(-1, -1);

			double BestAngle = MAX_dbl;
			for (const PCGExGraph::FLink Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (Lk.Edge == LockedEdge) { continue; }

				const FVector OtherDir = (PP - ProjectedPositions[InCluster->GetNode(NeighborIndex)->PointIndex]).GetSafeNormal();

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
				PCGExMath::CheckConvex(
					InCluster->GetPos(Nodes.Last(2)),
					InCluster->GetPos(Nodes.Last(1)),
					InCluster->GetPos(Nodes.Last()),
					bIsConvex, Sign);

				if (Constraints->bConvexOnly && !bIsConvex) { return ECellResult::WrongAspect; }
			}
		}

		if (NumUniqueNodes <= 2) { return ECellResult::Leaf; }

		if (!bIsClosedLoop) { return ECellResult::OpenCell; }
		if (!Constraints->IsUniqueCellHash(SharedThis(this))) { return ECellResult::Duplicate; }

		bBuiltSuccessfully = true;

		Centroid /= NumUniqueNodes;

		Perimeter = Metrics.Length;
		const double LastSegmentLength = FVector::Dist(InCluster->GetPos(Nodes[0]), InCluster->GetPos(Nodes.Last()));
		if (Constraints->MinSegmentLength > LastSegmentLength || LastSegmentLength > Constraints->MaxSegmentLength) { return ECellResult::OutsideSegmentsLimit; }

		if (Perimeter < Constraints->MinPerimeter || Perimeter > Constraints->MaxPerimeter) { return ECellResult::OutsidePerimeterLimit; }

		if (Constraints->bConcaveOnly && bIsConvex) { return ECellResult::WrongAspect; }
		if (NumUniqueNodes < Constraints->MinPointCount) { return ECellResult::OutsidePointsLimit; }
		if (Bounds.GetSize().Length() < Constraints->MinBoundsSize) { return ECellResult::OutsideBoundsLimit; }

		Polygon.Reset();
		TArray<FVector2D>& Vertices = *Polygon.Vertices;
		Vertices.SetNumUninitialized(Nodes.Num());
		for (int i = 0; i < Nodes.Num(); ++i) { Vertices[i] = FVector2D(ProjectedPositions[InCluster->GetNode(Nodes[i])->PointIndex]); }

		PCGExGeo::FPolygonInfos PolyInfos = PCGExGeo::FPolygonInfos(Vertices);

		Area = PolyInfos.Area;
		bIsClockwise = PolyInfos.bIsClockwise;
		Compactness = PolyInfos.Compactness;

		if (!PolyInfos.IsWinded(Constraints->Winding))
		{
			Algo::Reverse(Nodes);
			Algo::Reverse(Vertices);
		}

		if (Constraints->Holes && Constraints->Holes->Overlaps(Polygon)) { return ECellResult::Hole; }
		if (Constraints->MinCompactness > Compactness || Compactness > Constraints->MaxCompactness) { return ECellResult::OutsideCompactnessLimit; }

		Area *= 0.01; // QoL to avoid extra 000 in the detail panel.
		if (Constraints->MinArea > Area || Area > Constraints->MaxArea) { return ECellResult::OutsideAreaLimit; }

		// if (Constraints->WrapperCell && FMath::IsNearlyEqual(Area, Constraints->WrapperCell->Area, Constraints->WrapperClassificationTolerance)) { return ECellResult::WrapperCell; }

		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromCluster(
		const FVector& SeedPosition,
		const TSharedRef<PCGExCluster::FCluster>& InCluster,
		const TArray<FVector>& ProjectedPositions,
		const FPCGExNodeSelectionDetails* Picking)
	{
		PCGExGraph::FLink Link = PCGExGraph::FLink(-1, -1);
		Link.Node = InCluster->FindClosestNode<2>(SeedPosition, Picking ? Picking->PickingMethod : EPCGExClusterClosestSearchMode::Edge);

		if (Link.Node == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		if (const FVector StartPosition = InCluster->GetPos(Link.Node);
			Picking && !Picking->WithinDistance(StartPosition, SeedPosition))
		{
			// Fail. Not within radius.
			return ECellResult::Unknown;
		}

		Link.Edge = InCluster->FindClosestEdge<2>(Link.Node, SeedPosition);
		if (Link.Edge == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		Link.Node = InCluster->GetGuidedHalfEdge(Link.Edge, SeedPosition)->Index;
		return BuildFromCluster(Link, InCluster, ProjectedPositions);
	}

	ECellResult FCell::BuildFromPath(
		const TArray<FVector>& ProjectedPositions)
	{
		return ECellResult::Unknown;
	}

	void FCell::PostProcessPoints(TArray<FPCGPoint>& InMutablePoints)
	{
		if (!Constraints->bKeepCellsWithLeaves) { return; }

		const int32 NumPoints = InMutablePoints.Num();

		FVector A = InMutablePoints[0].Transform.GetLocation();
		FVector B = InMutablePoints[1].Transform.GetLocation();
		int32 BIdx = Nodes[0];
		int32 AIdx = -1;

		for (int i = 2; i < NumPoints; i++)
		{
			FVector C = InMutablePoints[i].Transform.GetLocation();

			if (B == C)
			{
				// Duplicate point, most likely a dead end. Could also be collocated points.
				//InMutablePoints[i].Transform.SetLocation(C + PCGExMath::GetNormalUp(A, B, FVector::UpVector) * 0.01); //Slightly offset
			}

			A = B;
			B = C;
		}
	}
}
