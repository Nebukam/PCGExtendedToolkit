// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Data/PCGExPointIO.h"
#include "PCGExEdgeRefineOperation.generated.h"

namespace PCGExCluster
{
	struct FVertex;
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
	virtual void PrepareForPointIO(PCGExData::FPointIO& PointIO);
	virtual void PrepareForCluster(PCGExData::FPointIO& EdgesIO, PCGExCluster::FCluster* InCluster);

	virtual void Cleanup() override;

protected:
	PCGExData::FPointIO* CurrentPoints = nullptr;
	PCGExData::FPointIO* CurrentEdges = nullptr;
	PCGExCluster::FCluster* CurrentCluster = nullptr;
};
