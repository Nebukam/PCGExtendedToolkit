﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Paths/PCGExPaths.h"

#include "PCGExCutClusters.generated.h"

UENUM()
enum class EPCGExCutEdgesMode : uint8
{
	Nodes         = 0 UMETA(DisplayName = "Nodes", ToolTip="Check for path overlap with nodes"),
	Edges         = 1 UMETA(DisplayName = "Edges", ToolTip="Check for path overlap with edges"),
	NodesAndEdges = 2 UMETA(DisplayName = "Edges & Nodes", ToolTip="Check for overlap with both nodes and edges"),
};

namespace PCGExCutEdges
{
	const FName SourceNodeFilters = FName("NodeFilters");
	const FName SourceEdgeFilters = FName("EdgeFilters");
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/paths-interop/cut-clusters"))
class UPCGExCutEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CutEdges, "Cluster : Cut", "Cut clusters nodes & edges using paths.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathEdgeIntersectionDetails IntersectionDetails = FPCGExPathEdgeIntersectionDetails(false);

	/** Keep intersections/proximity instead of removing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCutEdgesMode Mode = EPCGExCutEdgesMode::NodesAndEdges;

	/** If enabled, keep edges that connect two preserved nodes even if they don't intersect with the path. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bInvert && Mode!=EPCGExCutEdgesMode::Edges"))
	//bool bConservative = false;

	/** Expansion factor of node points to check for initial overlap. Uses scaled bounds expanded by the specified value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode != EPCGExCutEdgesMode::Edges"))
	double NodeExpansion = 1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode != EPCGExCutEdgesMode::Edges"))
	EPCGExDistance NodeDistanceSettings = EPCGExDistance::Center;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode != EPCGExCutEdgesMode::Edges"))
	bool bAffectedNodesAffectConnectedEdges = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bInvert && Mode != EPCGExCutEdgesMode::Nodes"))
	bool bAffectedEdgesAffectEndpoints = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bInvert && Mode != EPCGExCutEdgesMode::Edges"))
	bool bKeepEdgesThatConnectValidNodes = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExCutEdgesElement;
};

struct FPCGExCutEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExCutEdgesElement;

	TSharedPtr<PCGExDetails::FDistances> DistanceDetails;

	FPCGExPathEdgeIntersectionDetails IntersectionDetails;

	bool bWantsVtxProcessing = false;
	bool bWantsEdgesProcessing = false;

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> EdgeFilterFactories;
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> VtxFilterFactories;

	TArray<TSharedRef<PCGExData::FFacade>> PathFacades;
	TArray<TSharedRef<PCGExPaths::FPath>> Paths;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExCutEdgesElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CutEdges)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExCutEdges
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExCutEdgesContext, UPCGExCutEdgesSettings>
	{
	protected:
		int8 EdgesProcessed = 0;
		int8 NodesProcessed = 0;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			DefaultEdgeFilterValue = false;
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;

		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;

		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;

		virtual void OnEdgesProcessingComplete() override;
		virtual void OnNodesProcessingComplete() override;
		void TryConsolidate();
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(CutEdges)
			bRequiresGraphBuilder = true;
			bAllowVtxDataFacadeScopedGet = true;
			DefaultVtxFilterValue = false;
		}
	};
}
