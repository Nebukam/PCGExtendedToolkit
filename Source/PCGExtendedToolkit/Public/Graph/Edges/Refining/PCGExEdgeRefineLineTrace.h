// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExEdgeRefineLineTrace.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Line Trace"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineLineTrace : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool GetDefaultEdgeValidity() override { return !bInvert; }

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineLineTrace* TypedOther = Cast<UPCGExEdgeRefineLineTrace>(Other))
		{
			bTwoWayCheck = TypedOther->bTwoWayCheck;
			bInvert = TypedOther->bInvert;
			InitializedCollisionSettings = TypedOther->CollisionSettings;
			InitializedCollisionSettings.Init(TypedOther->Context);
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return true; }

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

		FPlatformAtomics::InterlockedExchange(&Edge.bValid, bInvert ? 1 : 0);
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	/** If the first linecast fails, tries the other way around. This is to ensure we don't fail against backfacing, but has high cost.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTwoWayCheck = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

protected:
	FPCGExCollisionDetails InitializedCollisionSettings;
};
