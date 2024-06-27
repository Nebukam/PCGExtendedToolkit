// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExEdgesProcessor.h"

#include "PCGExFindNodeStates.generated.h"

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFindNodeStatesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindNodeStates, "Graph : Find Node States", "Find & writes node states and attributes. Basically a glorified if/else to streamline identification of user-defined conditions within a graph.");
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	virtual int32 GetPreferredChunkSize() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Allow overlaping states to overlap. Useful if you want to evaluate multiple conditions separately and output the same final state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAllowStateOverlap = false;

	/** Write the name of the state to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteStateName = true;

	/** Name of the attribute to write state name to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateName"))
	FName StateNameAttributeName = "State";

	/** Name of the state to write if no conditions are met */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateName"))
	FName StatelessName = "None";


	/** Write the value of the state to an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteStateValue = true;

	/** Name of the attribute to write state name to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateValue"))
	FName StateValueAttributeName = "StateId";

	/** Name of the state to write if no conditions are met */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteStateValue"))
	int32 StatelessValue = -1;

	/** If enabled, will also write each state as a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bWriteEachStateIndividually = false;

private:
	friend class FPCGExFindNodeStatesElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFindNodeStatesContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExFindNodeStatesElement;

	virtual ~FPCGExFindNodeStatesContext() override;

	TArray<UPCGExNodeStateFactory*> StateFactories;
	PCGExDataState::TStatesManager* StatesManager = nullptr;
	TArray<int32> NodeIndices;
};


class PCGEXTENDEDTOOLKIT_API FPCGExFindNodeStatesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExFindNodeState
{
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FProcessorBatch;

		PCGExDataState::TStatesManager* StatesManager = nullptr;

	public:
		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
			bCacheVtxPointIndices = true;
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, TArrayView<PCGExData::FPointIO*> InEdges);
		virtual ~FProcessorBatch() override;

		virtual bool PrepareProcessing() override;
		virtual bool PrepareSingle(FProcessor* ClusterProcessor) override;
		virtual void Write() override;
	};
}
