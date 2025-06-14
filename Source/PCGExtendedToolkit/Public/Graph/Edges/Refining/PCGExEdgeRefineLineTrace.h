// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExEdgeRefineLineTrace.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Line Trace", PCGExNodeLibraryDoc="clusters/refine-cluster/line-trace"))
class UPCGExEdgeRefineLineTrace : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() override { return !bInvert; }

	// Required for initializing collision settings
	virtual bool CanOnlyExecuteOnMainThread() const override { return true; }

	virtual void InitializeInContext(FPCGExContext* InContext, FName InOverridesPinLabel) override
	{
		Super::InitializeInContext(InContext, InOverridesPinLabel);
		InitializedCollisionSettings = CollisionSettings;
		InitializedCollisionSettings.Init(InContext); // Needs to happen on main thread
	}

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineLineTrace* TypedOther = Cast<UPCGExEdgeRefineLineTrace>(Other))
		{
			bTwoWayCheck = TypedOther->bTwoWayCheck;
			bInvert = TypedOther->bInvert;
			InitializedCollisionSettings = TypedOther->InitializedCollisionSettings;
		}
	}

	virtual bool WantsIndividualEdgeProcessing() override { return true; }

	virtual void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& InHeuristics) override
	{
		Super::PrepareForCluster(InCluster, InHeuristics);
		ExchangeValue = bInvert ? 1 : 0;
	}

	virtual void ProcessEdge(PCGExGraph::FEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const FVector From = Cluster->GetStartPos(Edge);
		const FVector To = Cluster->GetEndPos(Edge);

		FHitResult HitResult;
		if (!InitializedCollisionSettings.Linecast(From, To, HitResult))
		{
			if (!bTwoWayCheck || !InitializedCollisionSettings.Linecast(To, From, HitResult)) { return; }
		}

		FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	/** If the first linecast fails, tries the other way around. This is to ensure we don't fail against backfacing, but has high cost.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTwoWayCheck = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	int8 ExchangeValue = 0;

protected:
	FPCGExCollisionDetails InitializedCollisionSettings;
};
