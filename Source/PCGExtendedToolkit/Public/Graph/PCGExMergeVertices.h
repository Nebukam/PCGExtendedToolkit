﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExClusterMT.h"
#include "PCGExEdgesProcessor.h"
#include "Data/PCGExPointIOMerger.h"


#include "PCGExMergeVertices.generated.h"


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/packing/merge-vtx"))
class UPCGExMergeVerticesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MergeVertices, "Cluster : Merge Vtx", "Merge Vtx so all edges share the same vtx collection.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterOp; }
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

struct FPCGExMergeVerticesContext final : FPCGExEdgesProcessorContext
{
	friend class UPCGExMergeVerticesSettings;
	friend class FPCGExMergeVerticesElement;

	FPCGExCarryOverDetails CarryOverDetails;

	PCGExCommon::DataIDType OutVtxId;
	TSharedPtr<PCGExData::FFacade> CompositeDataFacade;
	TSharedPtr<FPCGExPointIOMerger> Merger;

	virtual void ClusterProcessing_InitialProcessingDone() override;
	virtual void ClusterProcessing_WorkComplete() override;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExMergeVerticesElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(MergeVertices)

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

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
