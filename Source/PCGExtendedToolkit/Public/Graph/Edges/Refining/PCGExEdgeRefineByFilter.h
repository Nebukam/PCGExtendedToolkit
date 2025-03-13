// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineByFilter.generated.h"

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Filter"))
class UPCGExEdgeRefineByFilter : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() override { return true; }
	virtual bool RequiresIndividualEdgeProcessing() override { return true; }
	virtual bool GetDefaultEdgeValidity() override { return bInvert; }

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineByFilter* TypedOther = Cast<UPCGExEdgeRefineByFilter>(Other))
		{
			bInvert = TypedOther->bInvert;
		}
	}

	virtual void PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& InHeuristics) override
	{
		Super::PrepareForCluster(InCluster, InHeuristics);
		ExchangeValue = bInvert ? 0 : 1;
	}

	virtual void ProcessEdge(PCGExGraph::FEdge& Edge) override
	{
		if (*(EdgesFilters->GetData() + Edge.Index)) { FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue); }
	}

	int8 ExchangeValue = 0;

	/** If enabled, filtered out edges are kept, while edges that pass the filter are removed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};
