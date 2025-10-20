// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExMathMean.h"

#include "PCGExPathfinding.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Paths/PCGExPaths.h"

#include "PCGExPathfindingCentrality.generated.h"

class UPCGExSearchInstancedFactory;
/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="pathfinding/pathfinding-centrality"))
class UPCGExPathfindingCentralitySettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingCentrality, "Pathfinding : Centrality", "Compute betweenness centrality. Processing time increases exponentially with the number of vtx.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Search algorithm. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExSearchInstancedFactory> SearchAlgorithm;

	/** Name of the attribute */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName CentralityValueAttributeName = FName("Centrality");

	/** Discrete mode write the number as-is, relative will normalize against the highest number of overlaps found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Units", EditConditionHides))
	EPCGExMeanMeasure Units = EPCGExMeanMeasure::Discrete;

	/** Whether to do a OneMinus on the normalized overlap count value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ OneMinus", EditCondition="Units == EPCGExMeanMeasure::Relative", EditConditionHides))
	bool bOutputOneMinus = false;
	
	/** If disabled, will share memory allocations between queries, forcing them to execute one after another. Much slower, but very conservative for memory.  Using global feedback forces this behavior under the hood.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bGreedyQueries = true;
};

struct FPCGExPathfindingCentralityContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingCentralityElement;
	
	UPCGExSearchInstancedFactory* SearchAlgorithm = nullptr;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExPathfindingCentralityElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathfindingCentrality)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathfindingCentrality
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExPathfindingCentralityContext, UPCGExPathfindingCentralitySettings>
	{
		friend class FBatch;
		
	protected:
		TSharedPtr<PCGExPathfinding::FSearchAllocations> SearchAllocations;
		TArray<int32>* TraversalTracker = nullptr;
		
	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;
		
		TSharedPtr<FPCGExSearchOperation> SearchOperation;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;
		virtual void OnNodesProcessingComplete() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;
		TArray<int32> TraversalTracker;
		
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);

		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;

	};
}
