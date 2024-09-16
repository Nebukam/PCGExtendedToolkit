// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEdgeRefineOperation.h"
#include "PCGExEdgeRefineKeepByFilter.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, meta=(DisplayName="Keep by Filter"))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExEdgeKeepByFilter : public UPCGExEdgeRefineOperation
{
	GENERATED_BODY()

public:
	virtual bool SupportFilters() override { return true; }
	virtual bool GetDefaultEdgeValidity() override { return false; }
	virtual bool RequiresIndividualEdgeProcessing() override { return true; }

	virtual void ProcessEdge(PCGExGraph::FIndexedEdge& Edge) override
	{
		FPlatformAtomics::InterlockedExchange(&Edge.bValid, *(EdgesFilters->GetData() + Edge.EdgeIndex) ? 1 : 0);
	}
};
