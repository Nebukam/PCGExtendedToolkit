// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopology.h"

#include "PCGExCompare.h"

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
}

namespace PCGExTopology
{
	bool FCellConstraints::ContainsSignedEdgeHash(const uint64 Hash) const
	{
		if (!bDedupe) { return false; }
		FReadScopeLock ReadScopeLock(UniquePathsStartHashLock);
		return UniquePathsStartHash.Contains(Hash);
	}

	bool FCellConstraints::IsUniqueStartHash(const uint64 Hash)
	{
		if (!bDedupe) { return true; }
		bool bAlreadyExists = false;
		{
			FReadScopeLock ReadScopeLock(UniquePathsStartHashLock);
			bAlreadyExists = UniquePathsStartHash.Contains(Hash);
			if (bAlreadyExists) { return false; }
		}
		{
			FWriteScopeLock WriteScopeLock(UniquePathsStartHashLock);
			UniquePathsStartHash.Add(Hash, &bAlreadyExists);
			return !bAlreadyExists;
		}
	}

	bool FCellConstraints::IsUniqueCellHash(const FCell* InCell)
	{
		if (!bDedupe) { return true; }
		FWriteScopeLock WriteScopeLock(UniquePathsStartHashLock);

		TArray<int32> Nodes = InCell->Nodes;
		Nodes.Sort();

		uint32 Hash = 0;
		for (int32 i = 0; i < Nodes.Num(); i++) { Hash = HashCombineFast(Hash, Nodes[i]); }

		bool bAlreadyExists;
		UniquePathsBoxHash.Add(Hash, &bAlreadyExists);
		return !bAlreadyExists;
	}

	ECellResult FCell::BuildFromCluster(
		const int32 SeedNodeIndex,
		const int32 SeedEdgeIndex,
		const FVector& Guide,
		TSharedRef<PCGExCluster::FCluster> InCluster,
		const TArray<FVector>& ProjectedPositions,
		TSharedPtr<TArray<PCGExCluster::FExpandedNode>> ExpandedNodes)
	{
		bCompiledSuccessfully = false;
		Bounds = FBox(ForceInit);

		int32 StartNodeIndex = SeedNodeIndex;
		int32 PrevIndex = SeedNodeIndex;
		int32 NextIndex = InCluster->GetEdgeOtherNode(SeedEdgeIndex, PrevIndex)->NodeIndex;

		const FVector A = ProjectedPositions[InCluster->GetNode(PrevIndex)->PointIndex];
		const FVector B = ProjectedPositions[InCluster->GetNode(NextIndex)->PointIndex];

		const double SanityAngle = PCGExMath::GetDegreesBetweenVectors((B - A).GetSafeNormal(), (B - Guide).GetSafeNormal());
		const bool bStartsWithLeaf = InCluster->GetNode(StartNodeIndex)->IsLeaf();

		if (bStartsWithLeaf && !Constraints->bKeepCellsWithLeaves) { return ECellResult::Leaf; }

		if (SanityAngle > 180 && !bStartsWithLeaf)
		{
			// Swap search orientation
			PrevIndex = NextIndex;
			NextIndex = StartNodeIndex;
			StartNodeIndex = PrevIndex;
		}

		const uint64 UniqueStartEdgeHash = PCGEx::H64(PrevIndex, NextIndex);
		if (!Constraints->IsUniqueStartHash(UniqueStartEdgeHash)) { return ECellResult::Duplicate; }

		Bounds += InCluster->GetPos(PrevIndex);
		Nodes.Add(PrevIndex);

		int32 NumUniqueNodes = 1;

		TSet<int32> Exclusions = {PrevIndex, NextIndex};
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

			double BestAngle = -1;
			int32 NextBest = -1;

			const PCGExCluster::FNode* Current = InCluster->GetNode(NextIndex);

			Nodes.Add(Current->NodeIndex);
			NumUniqueNodes++;
			const FVector& RP = InCluster->GetPos(Current);
			Centroid += RP;

			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::ExceedPointsLimit; }

			Bounds += RP;
			if (Bounds.GetSize().Length() > Constraints->MaxBoundsSize) { return ECellResult::ExceedBoundsLimit; }

			const FVector PP = ProjectedPositions[Current->PointIndex];
			const FVector GuideDir = (PP - ProjectedPositions[InCluster->GetNode(PrevIndex)->PointIndex]).GetSafeNormal();

			if (Current->Num() == 1 && Constraints->bDuplicateLeafPoints) { Nodes.Add(Current->NodeIndex); }
			if (Current->Num() > 1) { Exclusions.Add(PrevIndex); }

			PrevIndex = NextIndex;

			bHasAdjacencyToStart = false;
			for (const PCGExGraph::FLink Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (NeighborIndex == StartNodeIndex) { bHasAdjacencyToStart = true; }
				if (Exclusions.Contains(NeighborIndex)) { continue; }

				const FVector OtherDir = (PP - ProjectedPositions[InCluster->GetNodePointIndex(NeighborIndex)]).GetSafeNormal();

				if (const double Angle = PCGExMath::GetDegreesBetweenVectors(OtherDir, GuideDir); Angle > BestAngle)
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
				if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::ExceedBoundsLimit; }

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
		Centroid /= NumUniqueNodes;

		if (Constraints->bClosedLoopOnly && !bIsClosedLoop) { return ECellResult::OpenCell; }
		if (Constraints->bConcaveOnly && bIsConvex) { return ECellResult::WrongAspect; }
		if (NumUniqueNodes < Constraints->MinPointCount) { return ECellResult::BelowPointsLimit; }
		if (Bounds.GetSize().Length() < Constraints->MinBoundsSize) { return ECellResult::BelowBoundsLimit; }
		if (Constraints->bDoWrapperCheck)
		{
			if (PCGExCompare::NearlyEqual(Bounds.GetSize().Length(), Constraints->DataBounds.GetSize().Length(), Constraints->WrapperCheckTolerance))
			{
				return ECellResult::ExceedBoundsLimit;
			}
		}
		if (!Constraints->IsUniqueCellHash(this)) { return ECellResult::Duplicate; }

		bCompiledSuccessfully = true;
		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromPath(const TArray<FVector>& ProjectedPositions)
	{
		return ECellResult::Unknown;
	}

	int32 FCell::GetTriangleNumEstimate() const
	{
		if (!bCompiledSuccessfully) { return 0; }
		if (bIsConvex || Nodes.Num() < 3) { return Nodes.Num(); }
		return Nodes.Num() + 2; // TODO : That's 100% arbitrary, need a better way to estimate concave triangulation
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
