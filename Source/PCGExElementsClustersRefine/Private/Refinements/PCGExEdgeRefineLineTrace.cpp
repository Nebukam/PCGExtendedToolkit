// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Refinements/PCGExEdgeRefineLineTrace.h"

#pragma region FPCGExEdgeRefineLineTrace

void FPCGExEdgeRefineLineTrace::PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHandler>& InHeuristics)
{
	FPCGExEdgeRefineOperation::PrepareForCluster(InCluster, InHeuristics);
	ExchangeValue = bInvert ? 1 : 0;
}

void FPCGExEdgeRefineLineTrace::ProcessEdge(PCGExGraphs::FEdge& Edge)
{
	FPCGExEdgeRefineOperation::ProcessEdge(Edge);

	const FVector From = Cluster->GetStartPos(Edge);
	const FVector To = Cluster->GetEndPos(Edge);

	if (bScatter)
	{
		bool bHit = false;
		for (int s = 0; s < ScatterSamples; ++s)
		{
			uint32 Seed = Edge.Start * 73856093 ^ Edge.End * 19349663 ^ s;
			bHit = InitializedCollisionSettings->Linecast(From, PCGExMath::RandomPointInSphere(To, ScatterRadius, Seed), bTwoWayCheck);
			if (bHit) { break; }
		}

		if (!bHit) { return; }
	}
	else
	{
		if (!InitializedCollisionSettings->Linecast(From, To, bTwoWayCheck)) { return; }
	}

	FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue);
}

#pragma endregion

#pragma region UPCGExEdgeRefineLineTrace

void UPCGExEdgeRefineLineTrace::InitializeInContext(FPCGExContext* InContext, FName InOverridesPinLabel)
{
	Super::InitializeInContext(InContext, InOverridesPinLabel);
	InitializedCollisionSettings = CollisionSettings;
	InitializedCollisionSettings.Init(InContext); // Needs to happen on main thread
}

void UPCGExEdgeRefineLineTrace::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExEdgeRefineLineTrace* TypedOther = Cast<UPCGExEdgeRefineLineTrace>(Other))
	{
		bTwoWayCheck = TypedOther->bTwoWayCheck;
		bInvert = TypedOther->bInvert;
		InitializedCollisionSettings = TypedOther->InitializedCollisionSettings;
	}
}

#pragma endregion
