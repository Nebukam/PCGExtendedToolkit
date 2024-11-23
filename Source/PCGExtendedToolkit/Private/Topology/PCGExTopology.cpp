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

	bool FCellConstraints::IsUniqueStartHash(const uint64 Hash)
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
		const int32 SeedNodeIndex,
		const int32 SeedEdgeIndex,
		TSharedRef<PCGExCluster::FCluster> InCluster,
		const TArray<FVector>& ProjectedPositions)
	{
		bBuiltSuccessfully = false;
		Bounds = FBox(ForceInit);

		int32 StartNodeIndex = SeedNodeIndex;
		int32 PrevIndex = SeedNodeIndex;
		int32 NextIndex = InCluster->GetEdgeOtherNode(SeedEdgeIndex, PrevIndex)->Index;

		const FVector A = ProjectedPositions[InCluster->GetNode(PrevIndex)->PointIndex];
		const FVector B = ProjectedPositions[InCluster->GetNode(NextIndex)->PointIndex];
		/*
				if (InCluster->GetNode(StartNodeIndex)->IsLeaf())
				{
					// Swap search orientation since we're starting with a dead end
					PrevIndex = NextIndex;
					NextIndex = StartNodeIndex;
					StartNodeIndex = PrevIndex;
				}
		*/
		SeedEdge = SeedEdgeIndex;
		SeedNode = StartNodeIndex;

		const uint64 UniqueStartEdgeHash = PCGEx::H64(PrevIndex, NextIndex);
		if (!Constraints->IsUniqueStartHash(UniqueStartEdgeHash)) { return ECellResult::Duplicate; }

		const FVector SeedRP = InCluster->GetPos(PrevIndex);

		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(SeedRP);
		Centroid = SeedRP;
		Bounds += SeedRP;

		Nodes.Add(PrevIndex);

		int32 NumUniqueNodes = 1;

		TSet<int32> Exclusions;
		if (!InCluster->GetNode(PrevIndex)->IsLeaf()) { Exclusions.Add(PrevIndex); }

		TSet<uint64> SignedEdges;
		bool bHasAdjacencyToStart = false;

		while (NextIndex != -1)
		{
			bool bEdgeAlreadyExists;
			uint64 SignedEdgeHash = PCGEx::H64(PrevIndex, NextIndex);

			//if (SignedEdgeHash != UniqueStartEdgeHash && Constraints->ContainsSignedEdgeHash(SignedEdgeHash)) { return ECellResult::Duplicate; }

			SignedEdges.Add(SignedEdgeHash, &bEdgeAlreadyExists);
			if (bEdgeAlreadyExists) { break; }

			//

			const PCGExCluster::FNode* Current = InCluster->GetNode(NextIndex);

			Nodes.Add(Current->Index);
			NumUniqueNodes++;

			const FVector& RP = InCluster->GetPos(Current);
			Centroid += RP;

			double SegmentLength = 0;
			const double NewLength = Metrics.Add(RP, SegmentLength);
			if (NewLength > Constraints->MaxPerimeter) { return ECellResult::Unknown; }
			if (SegmentLength < Constraints->MinSegmentLength || SegmentLength > Constraints->MaxSegmentLength) { return ECellResult::Unknown; }

			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::AbovePointsLimit; }

			Bounds += RP;
			if (Bounds.GetSize().Length() > Constraints->MaxBoundsSize) { return ECellResult::AboveBoundsLimit; }

			const FVector PP = ProjectedPositions[Current->PointIndex];
			const FVector GuideDir = (PP - ProjectedPositions[InCluster->GetNode(PrevIndex)->PointIndex]).GetSafeNormal();

			double BestAngle = MAX_dbl;
			int32 NextBest = -1;

			if (Current->Num() == 1 && Constraints->bDuplicateLeafPoints) { Nodes.Add(Current->Index); }
			if (Current->Num() > 1) { Exclusions.Add(PrevIndex); } // Only exclude previous if it's not a leaf, so we can wrap back

			PrevIndex = NextIndex;

			bHasAdjacencyToStart = false;
			for (const PCGExGraph::FLink Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (NeighborIndex == StartNodeIndex) { bHasAdjacencyToStart = true; }
				if (Exclusions.Contains(NeighborIndex)) { continue; }

				const FVector OtherDir = (PP - ProjectedPositions[InCluster->GetNode(NeighborIndex)->PointIndex]).GetSafeNormal();

				if (const double Angle = PCGExMath::GetRadiansBetweenVectors(OtherDir, GuideDir); Angle < BestAngle)
				{
					BestAngle = Angle;
					NextBest = NeighborIndex;
				}
			}

			Exclusions.Reset();

			if (NextBest == StartNodeIndex)
			{
				bHasAdjacencyToStart = true;
				NextBest = -1;
			}

			if (NextBest != -1)
			{
				if (InCluster->GetNode(NextBest)->Num() == 1 && !Constraints->bKeepCellsWithLeaves) { return ECellResult::Leaf; }
				if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::AboveBoundsLimit; }

				if (NumUniqueNodes > 2)
				{
					PCGExMath::CheckConvex(
						InCluster->GetPos(Nodes.Last(2)),
						InCluster->GetPos(Nodes.Last(1)),
						InCluster->GetPos(Nodes.Last()),
						bIsConvex, Sign);

					if (Constraints->bConvexOnly && !bIsConvex) { return ECellResult::WrongAspect; }
				}

				NextIndex = NextBest;
			}
			else
			{
				NextIndex = -1;
			}
		}

		if (NumUniqueNodes <= 2) { return ECellResult::Leaf; }

		bIsClosedLoop = bHasAdjacencyToStart;

		if (!bIsClosedLoop) { return ECellResult::OpenCell; }
		if (!Constraints->IsUniqueCellHash(SharedThis(this))) { return ECellResult::Duplicate; }

		bBuiltSuccessfully = true;

		Centroid /= NumUniqueNodes;

		Perimeter = Metrics.Length;
		double SegmentLength = 0;
		Perimeter = Metrics.Add(InCluster->GetPos(Nodes[0]), SegmentLength);
		if (SegmentLength < Constraints->MinSegmentLength || SegmentLength > Constraints->MaxSegmentLength) { return ECellResult::Unknown; }

		if (Perimeter < Constraints->MinPerimeter || Perimeter > Constraints->MaxPerimeter) { return ECellResult::Unknown; }

		if (Constraints->bConcaveOnly && bIsConvex) { return ECellResult::WrongAspect; }
		if (NumUniqueNodes < Constraints->MinPointCount) { return ECellResult::BelowPointsLimit; }
		if (Bounds.GetSize().Length() < Constraints->MinBoundsSize) { return ECellResult::BelowBoundsLimit; }

		Polygon.Reset();
		TArray<FVector2D>& Vertices = *Polygon.Vertices;
		Vertices.SetNumUninitialized(Nodes.Num());
		for (int i = 0; i < Nodes.Num(); ++i) { Vertices[i] = FVector2D(ProjectedPositions[InCluster->GetNode(Nodes[i])->PointIndex]); }

		Area = UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(Vertices);
		if (Area < 0)
		{
			Area = FMath::Abs(Area);
			Algo::Reverse(Nodes);
			Algo::Reverse(Vertices);
		}

		if (Constraints->Holes && Constraints->Holes->Overlaps(Polygon)) { return ECellResult::Hole; }

		if (Perimeter == 0.0f) { Compactness = 0; }
		else { Compactness = (4.0f * PI * Area) / (Perimeter * Perimeter); }

		if (Compactness < Constraints->MinCompactness || Compactness > Constraints->MaxCompactness) { return ECellResult::Unknown; }

		Area *= 0.01; // QoL to avoid extra 000 in the detail panel.
		if (Area < Constraints->MinArea) { return ECellResult::BelowAreaLimit; }
		if (Area > Constraints->MaxArea) { return ECellResult::AboveAreaLimit; }

		if (Constraints->WrapperCell && FMath::IsNearlyEqual(Area, Constraints->WrapperCell->Area, Constraints->WrapperClassificationTolerance)) { return ECellResult::WrapperCell; }

		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromCluster(
		const FVector& SeedPosition,
		const TSharedRef<PCGExCluster::FCluster>& InCluster,
		const TArray<FVector>& ProjectedPositions,
		const FPCGExNodeSelectionDetails* Picking)
	{
		int32 StartNodeIndex = InCluster->FindClosestNode<2>(SeedPosition, Picking ? Picking->PickingMethod : EPCGExClusterClosestSearchMode::Edge);

		if (StartNodeIndex == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		if (const FVector StartPosition = InCluster->GetPos(StartNodeIndex);
			Picking && !Picking->WithinDistance(StartPosition, SeedPosition))
		{
			// Fail. Not within radius.
			return ECellResult::Unknown;
		}

		const int32 NextEdge = InCluster->FindClosestEdge<2>(StartNodeIndex, SeedPosition);
		if (NextEdge == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		StartNodeIndex = InCluster->GetGuidedHalfEdge(NextEdge, SeedPosition)->Index;
		return BuildFromCluster(StartNodeIndex, NextEdge, InCluster, ProjectedPositions);
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
