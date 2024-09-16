// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineRemoveByFilter.generated.h"

namespace PCGExCluster
{
	struct FNode;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Remove by Filter"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeRemoveByFilter : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() override { return true; }
	virtual bool RequiresIndividualEdgeProcessing() override { return true; }

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		FPlatformAtomics::InterlockedExchange(&Edge.bValid, *(EdgesFilters->GetData() + Edge.EdgeIndex) ? 0 : 1);
	}
};
