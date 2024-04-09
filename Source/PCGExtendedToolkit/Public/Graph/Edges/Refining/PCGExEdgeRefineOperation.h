// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Data/PCGExPointIO.h"
#include "PCGExEdgeRefineOperation.generated.h"

namespace PCGExGraph
{
	class FGraph;
}

namespace PCGExCluster
{
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRefineOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForPointIO(PCGExData::FPointIO* InPointIO);
	virtual void PreProcess(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO);
	virtual void Process(PCGExCluster::FCluster* InCluster, PCGExGraph::FGraph* InGraph, PCGExData::FPointIO* InEdgesIO);

	virtual void Cleanup() override;

protected:
	PCGExData::FPointIO* PointIO;
};
