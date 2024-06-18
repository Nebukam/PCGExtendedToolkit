// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExSampleNeighbors.generated.h"

class UPCGExNeighborSampleOperation;

namespace PCGExSampleNeighbors
{
	PCGEX_ASYNC_STATE(State_ReadyForNextOperation)
	PCGEX_ASYNC_STATE(State_Sampling)
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNeighborsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNeighborsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNeighbors, "Sample : Neighbors", "Sample graph node' neighbors values.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

private:
	friend class FPCGExSampleNeighborsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExSampleNeighborsElement;
	virtual ~FPCGExSampleNeighborsContext() override;

	TArray<UPCGExNeighborSampleOperation*> SamplingOperations;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExSampleNeighbors
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{

		TArray<UPCGExNeighborSampleOperation*> SamplingOperations;

		TArray<UPCGExNeighborSampleOperation*> VtxOps;
		TArray<UPCGExNeighborSampleOperation*> EdgeOps;
		
	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges);
		virtual ~FProcessor() override;

		virtual bool Process(FPCGExAsyncManager* AsyncManager) override;
		virtual void ProcessSingleNode(PCGExCluster::FNode& Node) override;
		virtual void CompleteWork() override;
	};
}
