// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "Graph/PCGExCluster.h"

#include "PCGExEdgeRefineByAdjacency.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Refine Edge Threshold Mode"))
enum class EPCGExRefineEdgeThresholdMode : uint8
{
	Sum  = 0 UMETA(DisplayName = "Sum", Tooltip="The sum of adjacencies must be below the specified threshold"),
	Any  = 1 UMETA(DisplayName = "Any Endpoint", Tooltip="At least one endpoint adjacency count must be below the specified threshold"),
	Both = 2 UMETA(DisplayName = "Both Endpoints", Tooltip="Both endpoint adjacency count must be below the specified threshold"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine by Adjacency"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineByAdjacency : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineByAdjacency* TypedOther = Cast<UPCGExEdgeRefineByAdjacency>(Other))
		{
			Threshold = TypedOther->Threshold;
			Mode = TypedOther->Mode;
			bInvert = TypedOther->bInvert;
		}
	}

	virtual bool RequiresIndividualEdgeProcessing() override { return !bInvert; }

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		Super::ProcessEdge(Edge);

		const PCGExCluster::FNode* From = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.Start];
		const PCGExCluster::FNode* To = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.End];

		if (Mode == EPCGExRefineEdgeThresholdMode::Both)
		{
			if (From->Adjacency.Num() < Threshold && To->Adjacency.Num() < Threshold) { return; }
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Any)
		{
			if (From->Adjacency.Num() >= Threshold || To->Adjacency.Num() >= Threshold) { return; }
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Sum)
		{
			if ((From->Adjacency.Num() + To->Adjacency.Num()) < Threshold) { return; }
		}

		FPlatformAtomics::InterlockedExchange(&Edge.bValid, bInvert ? 1 : 0);
	}

	/** The number of connection endpoints must have to be considered a Bridge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin="1"))
	int32 Threshold = 2;

	/** How should we check if the threshold is reached. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExRefineEdgeThresholdMode Mode = EPCGExRefineEdgeThresholdMode::Sum;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};
