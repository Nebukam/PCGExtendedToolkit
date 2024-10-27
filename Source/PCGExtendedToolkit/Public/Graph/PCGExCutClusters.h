// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Paths/PCGExPaths.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"

#include "PCGExCutClusters.generated.h"

UENUM(/*E--BlueprintType, meta=(DisplayName="[PCGEx] Cut Edges Mode")--E*/)
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

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCutEdgesSettings : public UPCGExEdgesProcessorSettings
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
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Closed loop handling.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPathClosedLoopDetails ClosedLoop;


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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode!=EPCGExCutEdgesMode::Edges"))
	double NodeExpansion = 1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode!=EPCGExCutEdgesMode::Edges"))
	EPCGExDistance NodeDistanceSettings = EPCGExDistance::Center;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode!=EPCGExCutEdgesMode::Edges"))
	bool bAffectedNodesAffectConnectedEdges = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bInvert && Mode!=EPCGExCutEdgesMode::Nodes"))
	bool bAffectedEdgesAffectEndpoints = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bInvert && Mode!=EPCGExCutEdgesMode::Edges"))
	bool bKeepEdgesThatConnectValidNodes = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExCutEdgesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCutEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExCutEdgesElement;

	FPCGExPathClosedLoopDetails ClosedLoop;
	FPCGExPathEdgeIntersectionDetails IntersectionDetails;

	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> EdgeFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> NodeFilterFactories;

	TArray<TSharedRef<PCGExData::FFacade>> PathFacades;
	TArray<TSharedRef<PCGExPaths::FPath>> Paths;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCutEdgesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExCutEdges
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExCutEdgesContext, UPCGExCutEdgesSettings>
	{
	protected:
		TSharedPtr<PCGExClusterFilter::FManager> EdgeFilterManager;
		TSharedPtr<PCGExClusterFilter::FManager> NodeFilterManager;

		int8 EdgesProcessed = 0;
		int8 NodesProcessed = 0;

		TArray<bool> EdgeFilterCache;
		TArray<bool> NodeFilterCache;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		virtual void PrepareSingleLoopScopeForEdges(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count) override;
		virtual void PrepareSingleLoopScopeForNodes(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count) override;
		virtual void OnEdgesProcessingComplete() override;
		virtual void OnNodesProcessingComplete() override;
		void TryConsolidate();
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};

	class FProcessorBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch<FProcessor>(InContext, InVtx, InEdges)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(CutEdges)
			bRequiresGraphBuilder = true;
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
