// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"


#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Refining/PCGExEdgeRefineOperation.h"

#include "PCGExRefineEdges.generated.h"

namespace PCGExRefineEdges
{
	const FName SourceSanitizeEdgeFilters = FName("SanitizeFilters");
}

UENUM()
enum class EPCGExRefineSanitization : uint8
{
	None     = 0 UMETA(DisplayName = "None", ToolTip="No sanitization."),
	Shortest = 1 UMETA(DisplayName = "Shortest", ToolTip="If a node has no edge left, restore the shortest one."),
	Longest  = 2 UMETA(DisplayName = "Longest", ToolTip="If a node has no edge left, restore the longest one."),
	Filters  = 3 UMETA(DisplayName = "Filters", ToolTip="Use filters to find edges that must be preserved."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRefineEdgesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		RefineEdges, "Cluster : Refine", "Refine edges according to special rules.",
		(Refinement ? FName(Refinement.GetClass()->GetMetaData(TEXT("DisplayName"))) : FName("...")));
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExEdgeRefineOperation> Refinement;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bOutputEdgesOnly = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition = "bOutputEdgesOnly", EditConditionHides))
	bool bAllowZeroPointOutputs = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExRefineSanitization Sanitization = EPCGExRefineSanitization::None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	bool bRestoreEdgesThatConnectToValidNodes = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings", EditCondition="!bOutputEdgesOnly", EditConditionHides))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExRefineEdgesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRefineEdgesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExRefineEdgesElement;

	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> VtxFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> EdgeFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> SanitizationFilterFactories;

	UPCGExEdgeRefineOperation* Refinement = nullptr;

	TSharedPtr<PCGExData::FPointIOCollection> KeptEdges;
	TSharedPtr<PCGExData::FPointIOCollection> RemovedEdges;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExRefineEdgesElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExRefineEdges
{
	const FName SourceOverridesRefinement = TEXT("Overrides : Refinement");

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExRefineEdgesContext, UPCGExRefineEdgesSettings>
	{
		friend class FSanitizeRangeTask;
		friend class FFilterRangeTask;

	protected:
		TSharedPtr<PCGExClusterFilter::FManager> EdgeFilterManager;
		TSharedPtr<PCGExClusterFilter::FManager> SanitizationFilterManager;
		EPCGExRefineSanitization Sanitization = EPCGExRefineSanitization::None;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;
		mutable FRWLock NodeLock;

		TArray<int8> EdgeFilterCache;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope) override;

		virtual void PrepareSingleLoopScopeForEdges(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FEdge& Edge, const PCGExMT::FScope& Scope) override;
		virtual void OnEdgesProcessingComplete() override;
		void Sanitize();
		void InsertEdges() const;
		virtual void CompleteWork() override;

		UPCGExEdgeRefineOperation* Refinement = nullptr;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(RefineEdges)
			bRequiresGraphBuilder = !Settings->bOutputEdgesOnly;
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};

	class FSanitizeRangeTask final : public PCGExMT::FPCGExTask
	{
	public:
		FSanitizeRangeTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                   const TSharedPtr<FProcessor>& InProcessor):
			FPCGExTask(InPointIO),
			Processor(InProcessor)
		{
		}

		TSharedPtr<FProcessor> Processor;
		PCGExMT::FScope Scope = PCGExMT::FScope{};
		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
