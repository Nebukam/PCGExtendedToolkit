// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExEdgesProcessor.h"
#include "Data/PCGExPointIOMerger.h"


#include "PCGExMergeVertices.generated.h"


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMergeVerticesContext final : FPCGExEdgesProcessorContext
{
	friend class UPCGExMergeVerticesSettings;
	friend class FPCGExMergeVerticesElement;

	FPCGExCarryOverDetails CarryOverDetails;

	PCGExTags::IDType OutVtxId;
	TSharedPtr<PCGExData::FFacade> CompositeDataFacade;
	TSharedPtr<FPCGExPointIOMerger> Merger;

	virtual void ClusterProcessing_InitialProcessingDone() override;
	virtual void ClusterProcessing_WorkComplete() override;
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
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExMergeVerticesContext, UPCGExMergeVerticesSettings>
	{
		friend class FProcessorBatch;

	protected:
		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;

	public:
		int32 StartIndexOffset = 0;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
