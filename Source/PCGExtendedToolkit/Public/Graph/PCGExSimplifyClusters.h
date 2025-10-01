﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExChain.h"
#include "PCGExClusterMT.h"
#include "Details/PCGExDetailsIntersection.h"
#include "PCGExEdgesProcessor.h"
#include "Data/PCGExPointFilter.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "PCGExSimplifyClusters.generated.h"

UENUM()
enum class EPCGExSimplifyClusterEdgeFilterRole : uint8
{
	Preserve = 0 UMETA(DisplayName = "Preserve", ToolTip="Preserve endpoints of edges that pass the filters"),
	Collapse = 1 UMETA(DisplayName = "Collapse", ToolTip="Collapse endpoints of edges that pass the filters"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/simplify"))
class UPCGExSimplifyClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SimplifyClusters, "Cluster : Simplify", "Simplify connections by operating on isolated chains of nodes (only two neighbors).");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterOp; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface	
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceKeepConditionLabel, "Prevents vtx from being pruned by the simplification process", PCGExFactories::PointFilters, false)
	//~End UPCGExEdgesProcessorSettings interface

	/** If enabled, only check for dead ends. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOperateOnLeavesOnly = false;

	/**  Define the behavior of connected edge filters, if any */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSimplifyClusterEdgeFilterRole EdgeFilterRole = EPCGExSimplifyClusterEdgeFilterRole::Preserve;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bMergeAboveAngularThreshold = false;

	/** If enabled, uses an angular threshold below which nodes are merged. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bMergeAboveAngularThreshold", Units="Degrees", ClampMin=0, ClampMax=180))
	double AngularThreshold = 10;

	/** Removes hard angles instead of collinear ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Invert", EditCondition="bMergeAboveAngularThreshold", EditConditionHides, HideEditConditionToggle))
	bool bInvertAngularThreshold = false;

	/** If enabled, will consider collocated binary nodes for collocation and remove them as part of the simplification. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" ├─ Fuse Collocated", EditCondition="bMergeAboveAngularThreshold", EditConditionHides, HideEditConditionToggle))
	bool bFuseCollocated = true;

	/** Distance used to consider point to be overlapping. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Tolerance", ClampMin=0.001, EditCondition="bMergeAboveAngularThreshold && bFuseCollocated", EditConditionHides, HideEditConditionToggle))
	double FuseDistance = 0.001;

	/** If enabled, prune dead ends. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPruneLeaves = false;

	/** Defines how fused point properties and attributes are merged together for Edges (When an edge is the result of a simplification). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable))
	FPCGExBlendingDetails EdgeBlendingDetails;

	/** Meta filter settings for edge data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails EdgeCarryOverDetails;

	/**  Edge Union Data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending", meta=(PCG_Overridable))
	FPCGExEdgeUnionMetadataDetails EdgeUnionData;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct FPCGExSimplifyClustersContext : FPCGExEdgesProcessorContext
{
	friend class UPCGExSimplifyClustersSettings;
	friend class FPCGExSimplifyClustersElement;

	FPCGExCarryOverDetails EdgeCarryOverDetails;

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> EdgeFilterFactories;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExSimplifyClustersElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SimplifyClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExSimplifyClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExSimplifyClustersContext, UPCGExSimplifyClustersSettings>
	{
		friend class FBatch;

	protected:
		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;
		TSharedPtr<TArray<int8>> Breakpoints;
		TSharedPtr<PCGExCluster::FNodeChainBuilder> ChainBuilder;

		double FuseDistance = -1;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		// TODO : Register edge facade attribute dependencies

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void OnEdgesProcessingComplete() override;

		void CompileChains();
		virtual void CompleteWork() override;

		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;

		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	protected:
		PCGExGraph::FGraphMetadataDetails GraphMetadataDetails;
		TSharedPtr<TArray<int8>> Breakpoints;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
			bRequiresGraphBuilder = true;
		}

		virtual const PCGExGraph::FGraphMetadataDetails* GetGraphMetadataDetails() override;
		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
	};
}
