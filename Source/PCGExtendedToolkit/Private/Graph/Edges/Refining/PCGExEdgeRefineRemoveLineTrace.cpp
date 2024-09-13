// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Edges/Refining/PCGExEdgeRefineRemoveLineTrace.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"

void UPCGExEdgeRemoveLineTrace::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExEdgeRemoveLineTrace* TypedOther = Cast<UPCGExEdgeRemoveLineTrace>(Other))
	{
		bTwoWayCheck = TypedOther->bTwoWayCheck;
		InitializedCollisionSettings = TypedOther->CollisionSettings;
		InitializedCollisionSettings.Init(TypedOther->Context);
	}
}

void UPCGExEdgeRemoveLineTrace::ProcessEdge(PCGExGraph::FIndexedEdge& Edge)
{
	if (!*(EdgesFilters->GetData() + Edge.PointIndex)) { return; }

	Super::ProcessEdge(Edge);

	const FVector From = Cluster->GetPos((*Cluster->NodeIndexLookup)[Edge.Start]);
	const FVector To = Cluster->GetPos((*Cluster->NodeIndexLookup)[Edge.End]);

	FHitResult HitResult;
	if (!InitializedCollisionSettings.Linecast(From, To, HitResult))
	{
		if (!bTwoWayCheck || !InitializedCollisionSettings.Linecast(To, From, HitResult)) { return; }
	}

	FPlatformAtomics::InterlockedExchange(&Edge.bValid, 0);
}
