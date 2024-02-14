// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExGraph.h"
#include "PCGExEdgeRelaxingOperation.generated.h"

namespace PCGExCluster
{
	struct FNode;
}

namespace PCGExCluster
{
	struct FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class PCGEXTENDEDTOOLKIT_API UPCGExEdgeRelaxingOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void PrepareForPointIO(PCGExData::FPointIO& PointIO);
	virtual void PrepareForCluster(PCGExData::FPointIO& EdgesIO, PCGExCluster::FCluster* InCluster);
	virtual void PrepareForIteration(int Iteration, TArray<FVector>* PrimaryBuffer, TArray<FVector>* SecondaryBuffer);
	virtual void ProcessVertex(const PCGExCluster::FNode& Vertex);

	void ApplyInfluence(const PCGEx::FLocalSingleFieldGetter& Influence, TArray<FVector>* OverrideBuffer = nullptr) const;
	virtual void WriteActiveBuffer(PCGExData::FPointIO& PointIO);

	double DefaultInfluence = 1;

	virtual void Cleanup() override;

protected:
	int32 CurrentIteration = 0;
	PCGExData::FPointIO* CurrentPoints = nullptr;
	PCGExData::FPointIO* CurrentEdges = nullptr;
	PCGExCluster::FCluster* CurrentCluster = nullptr;
	TArray<FVector>* ReadBuffer = nullptr;
	TArray<FVector>* WriteBuffer = nullptr;
};
