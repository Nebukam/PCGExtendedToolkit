// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExEdgeRefineRemoveLineTrace.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove by Collision"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRemoveLineTrace : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRemoveLineTrace* TypedOther = Cast<UPCGExEdgeRemoveLineTrace>(Other))
		{
			bTwoWayCheck = TypedOther->bTwoWayCheck;
			InitializedCollisionSettings = TypedOther->CollisionSettings;
			InitializedCollisionSettings.Init(TypedOther->Context);
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return true; }

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExCollisionDetails CollisionSettings;

	/** If the first linecast fails, tries the other way around. This is to ensure we don't fail against backfacing, but has high cost.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bTwoWayCheck = true;

protected:
	FPCGExCollisionDetails InitializedCollisionSettings;
};
