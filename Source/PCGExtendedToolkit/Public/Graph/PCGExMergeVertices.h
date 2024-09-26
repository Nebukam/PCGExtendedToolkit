// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExEdgesProcessor.h"
#include "Data/PCGExPointIOMerger.h"


#include "PCGExMergeVertices.generated.h"


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMergeVerticesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergeVertices, "Cluster : Merge Vtx", "Merge Vtx so all edges share the same vtx collection.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergeVerticesContext final : public FPCGExEdgesProcessorContext
{
	friend class UPCGExMergeVerticesSettings;
	friend class FPCGExMergeVerticesElement;

	virtual ~FPCGExMergeVerticesContext() override;

	FPCGExCarryOverDetails CarryOverDetails;

	FString OutVtxId = TEXT("");
	TSharedPtr<PCGExData::FPointIO> CompositeIO;
	TUniquePtr<FPCGExPointIOMerger> Merger;

	virtual void OnBatchesProcessingDone() override;
	virtual void OnBatchesCompletingWorkDone() override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergeVerticesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExMergeVertices
{
	class FProcessor final : public PCGExClusterMT::TClusterProcessor<FPCGExMergeVerticesContext, UPCGExMergeVerticesSettings>
	{
		friend class FProcessorBatch;

	protected:
		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedPtr<PCGExCluster::FCluster>& InClusterRef) override;

	public:
		int32 StartIndexOffset = 0;

		FProcessor(const TSharedPtr<PCGExData::FPointIO>& InVtx, const TSharedPtr<PCGExData::FPointIO>& InEdges):
			TClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
