// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "Paths/PCGExPaths.h"

#include "PCGExPathfindingGrowPaths.generated.h"


struct FPCGExPathfindingGrowPathsContext;
class UPCGExPathfindingGrowPathsSettings;

UENUM()
enum class EPCGExGrowthIterationMode : uint8
{
	Parallel = 0 UMETA(DisplayName = "Parallel", ToolTip="Does one growth iteration on each seed until none remain"),
	Sequence = 1 UMETA(DisplayName = "Sequence", ToolTip="Grow a seed to its end, then move to the next seed"),
};

UENUM()
enum class EPCGExGrowthValueSource : uint8
{
	Constant      = 0 UMETA(DisplayName = "Constant", ToolTip="Use a single constant for all seeds"),
	SeedAttribute = 1 UMETA(DisplayName = "Seed Attribute", ToolTip="Attribute read on the seed."),
	VtxAttribute  = 2 UMETA(DisplayName = "Vtx Attribute", ToolTip="Attribute read on the vtx."),
};

UENUM()
enum class EPCGExGrowthUpdateMode : uint8
{
	Once             = 0 UMETA(DisplayName = "Once", ToolTip="Read once at the beginning of the computation."),
	SetEachIteration = 1 UMETA(DisplayName = "Set Each Iteration", ToolTip="Set the remaining number of iteration after each iteration."),
	AddEachIteration = 2 UMETA(DisplayName = "Add Each Iteration", ToolTip="Add to the remaning number of iterations after each iteration."),
};

namespace PCGExGrowPaths
{
	class FProcessor;

	class /*PCGEXTENDEDTOOLKIT_API*/ FGrowth
	{
	public:
		const TSharedPtr<FProcessor> Processor;
		const PCGExCluster::FNode* SeedNode = nullptr;
		TUniquePtr<PCGExCluster::FNode> GoalNode;
		TSharedPtr<PCGEx::FHashLookup> TravelStack;

		int32 SeedPointIndex = -1;
		int32 MaxIterations = 0;
		int32 SoftMaxIterations = 0;
		int32 Iteration = 0;
		int32 LastGrowthIndex = 0;
		int32 NextGrowthIndex = 0;
		int32 NextGrowthEdgeIndex = 0;
		FVector GrowthDirection = FVector::UpVector;
		PCGExPaths::FPathMetrics Metrics;

		double MaxDistance = 0;
		double Distance = 0;

		TArray<int32> Path;

		FGrowth(
			const TSharedPtr<FProcessor>& InProcessor,
			const int32 InMaxIterations,
			const int32 InLastGrowthIndex,
			const FVector& InGrowthDirection);

		int32 FindNextGrowthNodeIndex();
		bool Grow(); // return false if too far or couldn't connect for [reasons]
		void Write();

		~FGrowth()
		{
		}

	protected:
		void Init();
		double GetGrowthScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FEdge& Edge) const;
	};
}

class UPCGExHeuristicOperation;
class UPCGExSearchOperation;
/**
 * Use PCGExTransform to manipulate the outgoing attributes instead of handling everything here.
 * This way we can multi-thread the various calculations instead of mixing everything along with async/game thread collision
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathfindingGrowPathsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathfindingGrowPaths, "Pathfinding : Grow Paths", "Grow paths from seeds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPathfinding; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
public:
#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);

	/** Controls how iterative growth is managed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrowthIterationMode GrowthMode = EPCGExGrowthIterationMode::Parallel;

	/** The maximum number of growth iterations for a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrowthValueSource NumIterations = EPCGExGrowthValueSource::Constant;

	/** Num iteration attribute name. (will be translated to int32) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="NumIterations != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector NumIterationsAttribute;

	/** Num iteration constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="NumIterations == EPCGExGrowthValueSource::Constant", EditConditionHides))
	int32 NumIterationsConstant = 3;


	/** How to update the number of iteration for each seed.  Note: No matter what is selected, will never exceed the Max iteration. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="NumIterations != EPCGExGrowthValueSource::Constant && NumIterations == EPCGExGrowthValueSource::VtxAttribute", EditConditionHides))
	EPCGExGrowthUpdateMode NumIterationsUpdateMode = EPCGExGrowthUpdateMode::Once;

	/** The maximum number of growth started by a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrowthValueSource SeedNumBranches = EPCGExGrowthValueSource::Constant;

	/** How the NumBranches value is to be interpreted against the actual number of neighbors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExMeanMeasure SeedNumBranchesMean = EPCGExMeanMeasure::Discrete;

	/** Num branches constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedNumBranches == EPCGExGrowthValueSource::Constant", EditConditionHides))
	int32 NumBranchesConstant = 1;

	/** Num branches attribute name. (will be translated to int32) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SeedNumBranches != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector NumBranchesAttribute;


	/** The maximum number of growth iterations for a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrowthValueSource GrowthDirection = EPCGExGrowthValueSource::Constant;

	/** Growth direction attribute name. (will be translated to a FVector) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="GrowthDirection != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector GrowthDirectionAttribute;

	/** Growth direction constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="GrowthDirection == EPCGExGrowthValueSource::Constant", EditConditionHides))
	FVector GrowthDirectionConstant = FVector::UpVector;

	/** How to update the number of iteration for each seed.  Note: No matter what is selected, will never exceed the Max iteration. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrowthUpdateMode GrowthDirectionUpdateMode = EPCGExGrowthUpdateMode::Once;


	/** The maximum growth distance for a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGrowthValueSource GrowthMaxDistance = EPCGExGrowthValueSource::Constant;

	/** Max growth distance attribute name. (will be translated to a FVector) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="GrowthMaxDistance != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector GrowthMaxDistanceAttribute;

	/** Max growth distance constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="GrowthMaxDistance == EPCGExGrowthValueSource::Constant", EditConditionHides))
	double GrowthMaxDistanceConstant = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta = (PCG_Overridable))
	bool bUseGrowthStop = false;

	/** An attribute read on the Vtx as a boolean. If true and this node is used in a path, the path stops there. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta = (PCG_Overridable, EditCondition="bUseGrowthStop"))
	FPCGAttributePropertyInputSelector GrowthStopAttribute;

	/** Inverse Growth Stop behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta = (PCG_Overridable, EditCondition="bUseGrowthStop"))
	bool bInvertGrowthStop = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta = (PCG_Overridable))
	bool bUseNoGrowth = false;

	/** An attribute read on the Vtx as a boolean. If true, this point will never be grown on, but may be still used as seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta = (PCG_Overridable, EditCondition="bUseNoGrowth"))
	FPCGAttributePropertyInputSelector NoGrowthAttribute;

	/** Inverse No Growth behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits", meta = (PCG_Overridable, EditCondition="bUseNoGrowth"))
	bool bInvertNoGrowth = false;

	/** Visited weight threshold over which the growth is stopped if that's the only available option. -1 ignores.*/
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	//double VisitedStopThreshold = -1;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;

	/** Which Seed attributes to forward on paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails SeedForwarding;

	/** Output various statistics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	FPCGExPathStatistics Statistics;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or slightly slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Advanced")
	bool bUseOctreeSearch = false;
};


struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingGrowPathsContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingGrowPathsElement;

	TSharedPtr<PCGExData::FPointIOCollection> OutputPaths;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;

	TSharedPtr<PCGExData::TBuffer<int32>> NumIterations;
	TSharedPtr<PCGExData::TBuffer<int32>> NumBranches;
	TSharedPtr<PCGExData::TBuffer<FVector>> GrowthDirection;
	TSharedPtr<PCGExData::TBuffer<double>> GrowthMaxDistance;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingGrowPathsElement final : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExGrowPaths
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExPathfindingGrowPathsContext, UPCGExPathfindingGrowPathsSettings>
	{
		friend class FGrowth;
		friend class FProcessorBatch;

		TSharedPtr<PCGExData::TBuffer<int32>> NumIterations;
		TSharedPtr<PCGExData::TBuffer<int32>> NumBranches;
		TSharedPtr<PCGExData::TBuffer<FVector>> GrowthDirection;
		TSharedPtr<PCGExData::TBuffer<double>> GrowthMaxDistance;

		TSharedPtr<PCGExData::TBuffer<bool>> GrowthStop;
		TSharedPtr<PCGExData::TBuffer<bool>> NoGrowth;

	public:
		TArray<TSharedPtr<FGrowth>> Growths;
		TArray<TSharedPtr<FGrowth>> QueuedGrowths;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
		void Grow();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGrowTask final : public PCGExMT::FPCGExTask
	{
	public:
		FGrowTask(const TSharedPtr<FProcessor>& InProcessor) :
			FPCGExTask(),
			Processor(InProcessor)
		{
		}

		TSharedPtr<FProcessor> Processor;
		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const TSharedPtr<PCGExMT::FTaskGroup>& InGroup) override;
	};
}
