// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExClusterStates.h"

#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExFlagNodes.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFlagNodesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FlagNodes, "Cluster : Flag Nodes", "Find & writes node states as a int64 flag mask");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Attribute to output flags to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName FlagAttribute = FName("Flags");

	/** Initial flags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int64 InitialFlags;

private:
	friend class FPCGExFlagNodesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFlagNodesContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFlagNodesElement;

	virtual ~FPCGExFlagNodesContext() override;

	TArray<UPCGExFilterFactoryBase*> StateFactories;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFlagNodesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExFlagNodes
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FProcessorBatch;
		TArray<int64>* StateFlags = nullptr;
		PCGExClusterStates::FStateManager* StateManager = nullptr;

		bool bBuildExpandedNodes = false;
		TArray<PCGExCluster::FExpandedNode*>* ExpandedNodes = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
			bCacheVtxPointIndices = true;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		TArray<int64>* StateFlags = nullptr;

	public:
		FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);
		virtual ~FProcessorBatch() override;

		virtual bool PrepareProcessing() override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
	};
}
