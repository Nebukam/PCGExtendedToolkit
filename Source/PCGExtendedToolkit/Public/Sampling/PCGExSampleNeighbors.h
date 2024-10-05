// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExSampleNeighbors.generated.h"

namespace PCGExNeighborSample
{
	struct FNeighbor;
}

class UPCGExNeighborSamplerFactoryBase;
class UPCGExNeighborSampleOperation;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSampleNeighborsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNeighbors, "Cluster : Sample Neighbors", "Sample cluster vtx' neighbors values.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	//~Begin UPCGExEdgesProcessorSettings
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

private:
	friend class FPCGExSampleNeighborsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNeighborsContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExSampleNeighborsElement;
	TArray<TObjectPtr<const UPCGExNeighborSamplerFactoryBase>> SamplerFactories;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSampleNeighborsElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExSampleNeighbors
{
	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExSampleNeighborsContext, UPCGExSampleNeighborsSettings>
	{
		TArray<UPCGExNeighborSampleOperation*> SamplingOperations;
		TArray<UPCGExNeighborSampleOperation*> OpsWithValueTest;

		bool bBuildExpandedNodes = false;
		TSharedPtr<TArray<PCGExCluster::FExpandedNode>> ExpandedNodes;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TClusterProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
