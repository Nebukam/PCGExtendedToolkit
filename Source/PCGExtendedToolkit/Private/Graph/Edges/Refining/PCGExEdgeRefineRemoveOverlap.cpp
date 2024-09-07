// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveOverlap.h"

#include "Graph/PCGExCluster.h"
#include "Graph/PCGExIntersections.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"


void UPCGExEdgeRemoveOverlap::PrepareForCluster(PCGExCluster::FCluster* InCluster, PCGExHeuristics::THeuristicsHandler* InHeuristics)
{
	Super::PrepareForCluster(InCluster, InHeuristics);
	MinDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
	MaxDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
	ToleranceSquared = Tolerance * Tolerance;
	Cluster->GetExpandedEdges(true); // Let's hope it was cached ^_^
}

void UPCGExEdgeRemoveOverlap::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExEdgeRemoveOverlap* TypedOther = Cast<UPCGExEdgeRemoveOverlap>(Other))
	{
		Keep = TypedOther->Keep;
		Tolerance = TypedOther->Tolerance;
		bUseMinAngle = TypedOther->bUseMinAngle;
		MinAngle = TypedOther->MinAngle;
		bUseMaxAngle = TypedOther->bUseMaxAngle;
		MaxAngle = TypedOther->MaxAngle;
	}
}

void UPCGExEdgeRemoveOverlap::ProcessEdge(PCGExGraph::FIndexedEdge& Edge)
{
	const PCGExCluster::FExpandedEdge* EEdge = *(Cluster->ExpandedEdges->GetData() + Edge.EdgeIndex);
	const double Length = EEdge->GetEdgeLengthSquared(Cluster);

	auto ProcessOverlap = [&](const PCGExCluster::FClusterItemRef& ItemRef)
	{
		if (!Edge.bValid) { return false; }

		const PCGExCluster::FExpandedEdge* OtherEEdge = *(Cluster->ExpandedEdges->GetData() + ItemRef.ItemIndex);

		if (EEdge == OtherEEdge ||
			EEdge->Start == OtherEEdge->Start || EEdge->Start == OtherEEdge->End ||
			EEdge->End == OtherEEdge->End || EEdge->End == OtherEEdge->Start) { return true; }


		if (bUseMinAngle || bUseMaxAngle)
		{
			const double Dot = FMath::Abs(FVector::DotProduct(Cluster->GetDir(*EEdge->Start, *EEdge->End), Cluster->GetDir(*OtherEEdge->Start, *OtherEEdge->End)));
			if (!(Dot >= MaxDot && Dot <= MinDot)) { return true; }
		}

		const double OtherLength = OtherEEdge->GetEdgeLengthSquared(Cluster);

		FVector A;
		FVector B;
		if (Cluster->EdgeDistToEdgeSquared(EEdge->GetNodes(), OtherEEdge->GetNodes(), A, B) >= ToleranceSquared) { return true; }

		// Overlap!
		if (Keep == EPCGExEdgeOverlapPick::Longest)
		{
			if (OtherLength > Length)
			{
				FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
				return false;
			}
		}
		else
		{
			if (OtherLength < Length)
			{
				FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
				return false;
			}
		}

		return true;
	};

	Cluster->EdgeOctree->FindFirstElementWithBoundsTest(FBoxCenterAndExtent(EEdge->Bounds), ProcessOverlap);
}

/*
void UPCGExEdgeRemoveOverlap::Process()
{
	Super::Process();

	// Get a processing order by length first
	TMap<int32, int32> NodeIndexLookupRef = (*Cluster->NodeIndexLookup);
	TArray<int32> SortedEdgeIndices;
	PCGEx::ArrayOfIndices(SortedEdgeIndices, Cluster->Edges->Num());
	if (Keep == EPCGExEdgeOverlapPick::Longest)
	{
		SortedEdgeIndices.Sort(
			[&](const int32 A, const int32 B)
			{
				const double LA = *(Cluster->EdgeLengths->GetData() + A);
				const double LB = *(Cluster->EdgeLengths->GetData() + B);
				return LA > LB;
			});
	}
	else
	{
		SortedEdgeIndices.Sort(
			[&](const int32 A, const int32 B)
			{
				const double LA = *(Cluster->EdgeLengths->GetData() + A);
				const double LB = *(Cluster->EdgeLengths->GetData() + B);
				return LB > LA;
			});
	}

	for (const int32 EdgeIndex : SortedEdgeIndices)
	{
		PCGExGraph::FIndexedEdge* Edge = (Cluster->Edges->GetData() + EdgeIndex);

		if (!Edge->bValid) { continue; }

		const FVector Start = (Cluster->Nodes->GetData() + NodeIndexLookupRef[Edge->Start])->Position;
		const FVector End = (Cluster->Nodes->GetData() + NodeIndexLookupRef[Edge->End])->Position;
		const double Length = *(Cluster->EdgeLengths->GetData() + Edge->EdgeIndex);

		auto ProcessOverlap = [&](const PCGExCluster::FClusterItemRef& ItemRef)
		{
			if (!Edge->bValid) { return; }

			PCGExGraph::FIndexedEdge* OtherEdge = (Cluster->Edges->GetData() + ItemRef.ItemIndex);

			if (!OtherEdge->bValid || Edge == OtherEdge ||
				Edge->Start == OtherEdge->Start || Edge->Start == OtherEdge->End ||
				Edge->End == OtherEdge->End || Edge->End == OtherEdge->Start) { return; }

			// TODO: Check directions/dot
			const FVector OtherStart = (Cluster->Nodes->GetData() + NodeIndexLookupRef[OtherEdge->Start])->Position;
			const FVector OtherEnd = (Cluster->Nodes->GetData() + NodeIndexLookupRef[OtherEdge->End])->Position;
			const double OtherLength = *(Cluster->EdgeLengths->GetData() + OtherEdge->EdgeIndex);

			FVector A;
			FVector B;
			FMath::SegmentDistToSegment(
				Start, End,
				OtherStart, OtherEnd,
				A, B);

			if (FVector::DistSquared(A, B) >= ToleranceSquared) { return; }

			// Overlap!
			if (Keep == EPCGExEdgeOverlapPick::Longest)
			{
				if (Length > OtherLength)
				{
					// OtherEdge is invalid
					OtherEdge->bValid = false;
				}
				else
				{
					// Edge is invalid
					Edge->bValid = false;
				}
			}
			else
			{
				if (Length > OtherLength)
				{
					// Edge is invalid
					Edge->bValid = false;
				}
				else
				{
					// OtherEdge is invalid
					OtherEdge->bValid = false;
				}
			}
		};

		Cluster->EdgeOctree->FindElementsWithBoundsTest(FBoxCenterAndExtent(FMath::Lerp(Start, End, 0.5), FVector(Length * 0.5)), ProcessOverlap);
	}
}
*/
