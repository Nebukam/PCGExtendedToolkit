// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "PCGExEdgeRefineOperation.h"
#include "Details/PCGExCollisionDetails.h"
#include "Clusters/PCGExCluster.h"
#include "Math/PCGExMath.h"
#include "PCGExEdgeRefineLineTrace.generated.h"

struct FPCGExCollisionDetails;
/**
 * 
 */
class FPCGExEdgeRefineLineTrace : public FPCGExEdgeRefineOperation
{
public:
	virtual void PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHandler>& InHeuristics) override
	{
		FPCGExEdgeRefineOperation::PrepareForCluster(InCluster, InHeuristics);
		ExchangeValue = bInvert ? 1 : 0;
	}

	virtual void ProcessEdge(PCGExGraphs::FEdge& Edge) override
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

	bool bTwoWayCheck = true;
	bool bInvert = false;
	int8 ExchangeValue = 0;

	bool bScatter = false;
	double ScatterSamples = 10;
	double ScatterRadius = 10;

	const FPCGExCollisionDetails* InitializedCollisionSettings = nullptr;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Line Trace", PCGExNodeLibraryDoc="clusters/refine-cluster/line-trace"))
class UPCGExEdgeRefineLineTrace : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() const override { return !bInvert; }

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

	virtual bool WantsIndividualEdgeProcessing() const override { return true; }

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	/** If the first linecast fails, tries the other way around. This is to ensure we don't fail against backfacing, but has high cost.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTwoWayCheck = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bScatter = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Samples", EditCondition="bScatter", EditConditionHides, ClampMin=1))
	double ScatterSamples = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Radius", EditCondition="bScatter", EditConditionHides))
	double ScatterRadius = 10;


	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRefineLineTrace, { Operation->bTwoWayCheck = bTwoWayCheck; Operation->bInvert = bInvert; Operation->InitializedCollisionSettings = &InitializedCollisionSettings; Operation->bScatter = bScatter; Operation->ScatterSamples = FMath::Max(1, ScatterSamples); Operation->ScatterRadius = ScatterRadius; })

protected:
	FPCGExCollisionDetails InitializedCollisionSettings;
};
