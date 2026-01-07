// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineByFilter.generated.h"

namespace PCGExClusters
{
	struct FNode;
}

/**
 * 
 */
class FPCGExEdgeRefineByFilter : public FPCGExEdgeRefineOperation
{
public:
	virtual void PrepareForCluster(const TSharedPtr<PCGExClusters::FCluster>& InCluster, const TSharedPtr<PCGExHeuristics::FHandler>& InHeuristics) override
	{
		FPCGExEdgeRefineOperation::PrepareForCluster(InCluster, InHeuristics);
		ExchangeValue = bInvert ? 0 : 1;
	}

	virtual void ProcessEdge(PCGExGraphs::FEdge& Edge) override
	{
		if (*(EdgeFilterCache->GetData() + Edge.Index)) { FPlatformAtomics::InterlockedExchange(&Edge.bValid, ExchangeValue); }
	}

	int8 ExchangeValue = 0;
	bool bInvert = false;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Refine : Filter", PCGExNodeLibraryDoc="clusters/refine-cluster/filters"))
class UPCGExEdgeRefineByFilter : public UPCGExEdgeRefineInstancedFactory
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() const override { return true; }
	virtual bool WantsIndividualEdgeProcessing() const override { return true; }
	virtual bool GetDefaultEdgeValidity() const override { return bInvert; }

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override
	{
		Super::CopySettingsFrom(Other);
		if (const UPCGExEdgeRefineByFilter* TypedOther = Cast<UPCGExEdgeRefineByFilter>(Other))
		{
			bInvert = TypedOther->bInvert;
		}
	}

	/** If enabled, filtered out edges are kept, while edges that pass the filter are removed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	PCGEX_CREATE_REFINE_OPERATION(EdgeRefineByFilter, { Operation->bInvert = bInvert; })
};
