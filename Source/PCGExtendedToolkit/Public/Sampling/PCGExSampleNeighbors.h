// Copyright 2024 Timothé Lapetite and contributors
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

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	//~Begin UPCGExEdgesProcessorSettings
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
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
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExSampleNeighborsContext, UPCGExSampleNeighborsSettings>
	{
		TArray<UPCGExNeighborSampleOperation*> SamplingOperations;
		TArray<UPCGExNeighborSampleOperation*> OpsWithValueTest;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNeighbors)
			bRequiresWriteStep = true;
			bWriteVtxDataFacade = true;
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
	};
}
