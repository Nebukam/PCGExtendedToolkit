// Copyright Timothé Lapetite 2024
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRefineByFilter : public UPCGExEdgeRefineOperation
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

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		FPlatformAtomics::InterlockedExchange(&Edge.bValid, *(EdgesFilters->GetData() + Edge.EdgeIndex) ? !bInvert : bInvert);
	}

	/** If enabled, filtered out edges are kept, while edges that pass the filter are removed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;
};
