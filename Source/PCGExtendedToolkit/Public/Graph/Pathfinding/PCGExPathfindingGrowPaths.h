// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPathfinding.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExPathfindingGrowPaths.generated.h"

class UPCGExVtxPropertyOperation;

namespace PCGExHeuristics
{
	class THeuristicsHandler;
}

struct FPCGExPathfindingGrowPathsContext;
class UPCGExPathfindingGrowPathsSettings;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Growth Value Source"))
enum class EPCGExGrowthIterationMode : uint8
{
	Parallel UMETA(DisplayName = "Parallel", ToolTip="Does one growth iteration on each seed until none remain"),
	Sequence UMETA(DisplayName = "Sequence", ToolTip="Grow a seed to its end, then move to the next seed"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Growth Value Source"))
enum class EPCGExGrowthValueSource : uint8
{
	Constant UMETA(DisplayName = "Constant", ToolTip="Use a single constant for all seeds"),
	SeedAttribute UMETA(DisplayName = "Seed Attribute", ToolTip="Attribute read on the seed."),
	VtxAttribute UMETA(DisplayName = "Vtx Attribute", ToolTip="Attribute read on the vtx."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Growth Update Mode"))
enum class EPCGExGrowthUpdateMode : uint8
{
	Once UMETA(DisplayName = "Once", ToolTip="Read once at the beginning of the computation."),
	SetEachIteration UMETA(DisplayName = "Set Each Iteration", ToolTip="Set the remaining number of iteration after each iteration."),
	AddEachIteration UMETA(DisplayName = "Add Each Iteration", ToolTip="Add to the remaning number of iterations after each iteration."),
};

namespace PCGExGrowPaths
{
	class FProcessor;

	class /*PCGEXTENDEDTOOLKIT_API*/ FGrowth
	{
	public:
		const FProcessor* Processor = nullptr;
		const FPCGExPathfindingGrowPathsContext* Context = nullptr;
		const UPCGExPathfindingGrowPathsSettings* Settings = nullptr;
		const PCGExCluster::FNode* SeedNode = nullptr;
		PCGExCluster::FNode* GoalNode = nullptr;

		int32 SeedPointIndex = -1;
		int32 MaxIterations = 0;
		int32 SoftMaxIterations = 0;
		int32 Iteration = 0;
		int32 LastGrowthIndex = 0;
		int32 NextGrowthIndex = 0;
		FVector GrowthDirection = FVector::UpVector;
		PCGExMath::FPathMetrics Metrics;

		double MaxDistance = 0;
		double Distance = 0;

		TArray<int32> Path;

		FGrowth(
			const FProcessor* InProcessor,
			const UPCGExPathfindingGrowPathsSettings* InSettings,
			const int32 InMaxIterations,
			const int32 InLastGrowthIndex,
			const FVector& InGrowthDirection);

		int32 FindNextGrowthNodeIndex();
		bool Grow(); // return false if too far or couldn't connect for [reasons]
		void Write();

		~FGrowth()
		{
			Path.Empty();
			PCGEX_DELETE(GoalNode)
		}

	protected:
		void Init();
		double GetGrowthScore(
			const PCGExCluster::FNode& From,
			const PCGExCluster::FNode& To,
			const PCGExGraph::FIndexedEdge& Edge) const;
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

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** Controls how iterative growth is managed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExGrowthIterationMode GrowthMode = EPCGExGrowthIterationMode::Parallel;

	/** The maximum number of growth iterations for a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExGrowthValueSource NumIterations = EPCGExGrowthValueSource::Constant;

	/** Num iteration attribute name. (will be translated to int32) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="NumIterations != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector NumIterationsAttribute;

	/** Num iteration constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="NumIterations == EPCGExGrowthValueSource::Constant", EditConditionHides))
	int32 NumIterationsConstant = 3;


	/** How to update the number of iteration for each seed. \n Note: No matter what is selected, will never exceed the Max iteration. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="NumIterations != EPCGExGrowthValueSource::Constant && NumIterations == EPCGExGrowthValueSource::VtxAttribute", EditConditionHides))
	EPCGExGrowthUpdateMode NumIterationsUpdateMode = EPCGExGrowthUpdateMode::Once;

	/** The maximum number of growth started by a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExGrowthValueSource SeedNumBranches = EPCGExGrowthValueSource::Constant;

	/** How the NumBranches value is to be interpreted against the actual number of neighbors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExMeanMeasure SeedNumBranchesMean = EPCGExMeanMeasure::Discrete;

	/** Num branches constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="SeedNumBranches == EPCGExGrowthValueSource::Constant", EditConditionHides))
	int32 NumBranchesConstant = 1;

	/** Num branches attribute name. (will be translated to int32) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="SeedNumBranches != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector NumBranchesAttribute;


	/** The maximum number of growth iterations for a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExGrowthValueSource GrowthDirection = EPCGExGrowthValueSource::Constant;

	/** Growth direction attribute name. (will be translated to a FVector) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="GrowthDirection != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector GrowthDirectionAttribute;

	/** Growth direction constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="GrowthDirection == EPCGExGrowthValueSource::Constant", EditConditionHides))
	FVector GrowthDirectionConstant = FVector::UpVector;

	/** How to update the number of iteration for each seed. \n Note: No matter what is selected, will never exceed the Max iteration. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExGrowthUpdateMode GrowthDirectionUpdateMode = EPCGExGrowthUpdateMode::Once;


	/** The maximum growth distance for a given seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth")
	EPCGExGrowthValueSource GrowthMaxDistance = EPCGExGrowthValueSource::Constant;

	/** Max growth distance attribute name. (will be translated to a FVector) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="GrowthMaxDistance != EPCGExGrowthValueSource::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector GrowthMaxDistanceAttribute;

	/** Max growth distance constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth", meta = (PCG_Overridable, EditCondition="GrowthMaxDistance == EPCGExGrowthValueSource::Constant", EditConditionHides))
	double GrowthMaxDistanceConstant = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth|Limits", meta = (PCG_Overridable))
	bool bUseGrowthStop = false;

	/** An attribute read on the Vtx as a boolean. If true and this node is used in a path, the path stops there. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth|Limits", meta = (PCG_Overridable, EditCondition="bUseGrowthStop"))
	FPCGAttributePropertyInputSelector GrowthStopAttribute;

	/** Inverse Growth Stop behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth|Limits", meta = (PCG_Overridable, EditCondition="bUseGrowthStop"))
	bool bInvertGrowthStop = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth|Limits", meta = (PCG_Overridable))
	bool bUseNoGrowth = false;

	/** An attribute read on the Vtx as a boolean. If true, this point will never be grown on, but may be still used as seed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth|Limits", meta = (PCG_Overridable, EditCondition="bUseNoGrowth"))
	FPCGAttributePropertyInputSelector NoGrowthAttribute;

	/** Inverse No Growth behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Growth|Limits", meta = (PCG_Overridable, EditCondition="bUseNoGrowth"))
	bool bInvertNoGrowth = false;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Heuristics", meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);

	/** Visited weight threshold over which the growth is stopped if that's the only available option. -1 ignores.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Extra Weighting", meta=(EditCondition="bWeightUpVisited"))
	double VisitedStopThreshold = -1;

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


struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathfindingGrowPathsContext final : public FPCGExEdgesProcessorContext
{
	friend class FPCGExPathfindingGrowPathsElement;

	virtual ~FPCGExPathfindingGrowPathsContext() override;

	PCGExData::FPointIOCollection* OutputPaths = nullptr;

	PCGExData::FFacade* SeedsDataFacade = nullptr;

	PCGExData::FCache<int32>* NumIterations = nullptr;
	PCGExData::FCache<int32>* NumBranches = nullptr;
	PCGExData::FCache<FVector>* GrowthDirection = nullptr;
	PCGExData::FCache<double>* GrowthMaxDistance = nullptr;

	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	PCGExData::FDataForwardHandler* SeedForwardHandler = nullptr;
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
	class FProcessor final : public PCGExClusterMT::FClusterProcessor
	{
		friend class FGrowth;
		friend class FProcessorBatch;

		PCGExData::FCache<int32>* NumIterations = nullptr;
		PCGExData::FCache<int32>* NumBranches = nullptr;
		PCGExData::FCache<FVector>* GrowthDirection = nullptr;
		PCGExData::FCache<double>* GrowthMaxDistance = nullptr;

		PCGExData::FCache<bool>* GrowthStop = nullptr;
		PCGExData::FCache<bool>* NoGrowth = nullptr;

	public:
		TArray<FGrowth*> Growths;
		TArray<FGrowth*> QueuedGrowths;

		FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
			FClusterProcessor(InVtx, InEdges)
		{
		}

		virtual ~FProcessor() override
		{
			Growths.Empty();
			QueuedGrowths.Empty();
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
		void Grow();
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FGrowTask final : public PCGExMT::FPCGExTask
	{
	public:
		FGrowTask(PCGExData::FPointIO* InPointIO, FProcessor* InProcessor) :
			FPCGExTask(InPointIO), Processor(InProcessor)
		{
		}

		FProcessor* Processor = nullptr;
		virtual bool ExecuteTask() override;
	};
}
