// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExScopedContainers.h"
#include "Data/PCGExDataForward.h"
#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExClusterDiffusion.generated.h"

#define PCGEX_FOREACH_FIELD_CLUSTER_DIFF(MACRO)\
MACRO(DiffusionDepth, int32, 0)\
MACRO(DiffusionOrder, int32, 0)\
MACRO(DiffusionDistance, double, 0)

UENUM()
enum class EPCGExDiffusionOrder : uint8
{
	Index   = 0 UMETA(DisplayName = "Index", ToolTip="Uses point index to drive diffusion order."),
	Sorting = 1 UMETA(DisplayName = "Sorting", ToolTip="Use sorting rules to drive diffusion order."),
};

UENUM()
enum class EPCGExDiffusionSeedsSource : uint8
{
	Filters = 0 UMETA(DisplayName = "Filters", ToolTip="Uses filters to pick which vtx should be used as seeds."),
	Points  = 1 UMETA(DisplayName = "Points", ToolTip="Uses source seed points as seed, and picks the closest vtx."),
};

UENUM()
enum class EPCGExDiffusionProcessing : uint8
{
	Parallel = 0 UMETA(DisplayName = "Parallel", ToolTip="Diffuse each vtx once before moving to the next iteration."),
	Sequence = 1 UMETA(DisplayName = "Sequential", ToolTip="Diffuse each vtx until it stops before moving to the next one, and so on."),
};

UENUM()
enum class EPCGExDiffusionPrioritization : uint8
{
	Heuristics = 0 UMETA(DisplayName = "Heuristics", ToolTip="Prioritize expansion based on heuristics first, then depth."),
	Depth      = 1 UMETA(DisplayName = "Depth", ToolTip="Prioritize expansion based on depth, then heuristics."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDiffusionSeedPickingDetails
{
	GENERATED_BODY()

	FPCGExDiffusionSeedPickingDetails()
	{
	}

	/** Defines the type of seeds used for diffusion. Points lets you use a single point dataset as spatial input, while filters will use filters on the vtx to determine which vtx should be diffused. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDiffusionSeedsSource Source = EPCGExDiffusionSeedsSource::Points;

	/** Drive how a seed point selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Source==EPCGExDiffusionSeedsSource::Points", EditConditionHides))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);

	/** Defines the sorting used for the vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Source==EPCGExDiffusionSeedsSource::Filters", EditConditionHides))
	EPCGExDiffusionOrder Ordering = EPCGExDiffusionOrder::Index;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Source==EPCGExDiffusionSeedsSource::Filters", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDiffusionPrioritizationDetails
{
	GENERATED_BODY()

	FPCGExDiffusionPrioritizationDetails()
	{
	}

	/** Defines how each vtx is diffused */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDiffusionProcessing Processing = EPCGExDiffusionProcessing::Parallel;
	
	/** Which data should be prioritized to 'drive' diffusion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDiffusionPrioritization Priority = EPCGExDiffusionPrioritization::Heuristics;

	/** Sort direction. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Seeds==EPCGExDiffusionSeeds::Filters", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Diffusion Rate type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Processing==EPCGExDiffusionProcessing::Parallel", EditConditionHides))
	EPCGExInputValueType DiffusionRateInput = EPCGExInputValueType::Constant;

	/** Fetch the Diffusion Rate from a local attribute. Must be >= 0, but zero wont grow -- it will however "preserve" the vtx from being diffused on. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Diffusion Rate (Attr)", EditCondition="Processing==EPCGExDiffusionProcessing::Parallel && DiffusionRateInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName DiffusionRateAttribute = FName("DiffusionRate");

	/** Diffusion rate constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Diffusion Rate", EditCondition="Processing==EPCGExDiffusionProcessing::Parallel && DiffusionRateInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	int32 DiffusionRateConstant = 1;

	/** If enabled, heuristics score will be accumulated similar to pathfinding. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bAccumulateHeuristics = true;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExClusterDiffusionSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ClusterDiffusion, "Cluster : Diffusion", "Diffuses vtx attributes onto their neighbors.");
#endif

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Seeds settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExDiffusionSeedPickingDetails Seeds;

	/** Diffusion settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExDiffusionPrioritizationDetails Diffusion;

#pragma region Limits

	// Max count

	/** Whether to limit the number of vtx that will be defused to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Count", meta=(PCG_NotOverridable))
	bool bUseMaxCount = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Count", meta=(PCG_NotOverridable, EditCondition="bUseMaxCount", EditConditionHides))
	EPCGExInputValueType MaxCountInput = EPCGExInputValueType::Constant;

	/** Max count Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Count", meta=(PCG_Overridable, DisplayName="Max Count (Attr)", EditCondition="bUseMaxCount && MaxCountInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxCountAttribute = FName("MaxCount");

	/** Max count Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Count", meta=(PCG_Overridable, DisplayName="Max Count", EditCondition="bUseMaxCount && MaxCountInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	double MaxCount = 10;

	// Max depth

	/** Whether to limit the connectivity depth of vtx that will be defused to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Depth", meta=(PCG_NotOverridable))
	bool bUseMaxDepth = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Depth", meta=(PCG_NotOverridable, EditCondition="bUseMaxDepth", EditConditionHides))
	EPCGExInputValueType MaxDepthInput = EPCGExInputValueType::Constant;

	/** Max depth Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Depth", meta=(PCG_Overridable, DisplayName="Max Depth (Attr)", EditCondition="bUseMaxDepth && MaxDepthInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxDepthAttribute = FName("MaxDepth");

	/** Max depth Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Depth", meta=(PCG_Overridable, DisplayName="Max Depth", EditCondition="bUseMaxDepth && MaxDepthInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	double MaxDepth = 10;
	
	// Max length

	/** Whether to limit the length of the individual growths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Length", meta=(PCG_NotOverridable))
	bool bUseMaxLength = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Length", meta=(PCG_NotOverridable, EditCondition="bUseMaxLength", EditConditionHides))
	EPCGExInputValueType MaxLengthInput = EPCGExInputValueType::Constant;

	/** Max length Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Length", meta=(PCG_Overridable, DisplayName="Max Length (Attr)", EditCondition="bUseMaxLength && MaxLengthInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxLengthAttribute = FName("MaxLength");

	/** Max length Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Length", meta=(PCG_Overridable, DisplayName="Max Length", EditCondition="bUseMaxLength && MaxLengthInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	double MaxLength = 100;

	// Max Radius

	/** Whether to limit the expansion within a maximum radius of the vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Radius", meta=(PCG_NotOverridable))
	bool bUseMaxRadius = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Radius", meta=(PCG_NotOverridable, EditCondition="bUseMaxRadius", EditConditionHides))
	EPCGExInputValueType MaxRadiusInput = EPCGExInputValueType::Constant;

	/** Max radius Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Radius", meta=(PCG_Overridable, DisplayName="Max Radius (Attr)", EditCondition="bUseMaxRadius && MaxRadiusInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxRadiusAttribute = FName("MaxRadius");

	/** Max radius Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Radius", meta=(PCG_Overridable, DisplayName="Max Radius", EditCondition="bUseMaxRadius && MaxRadiusInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	double MaxRadius = 100;

	// Other

	/** Whether to limit candidate to vtxs that are inside the bounds of the seeds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Other", meta=(PCG_NotOverridable))
	bool bLimitToSeedBounds = false;

	// Max influences

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Other", meta=(PCG_NotOverridable))
	EPCGExInputValueType MaxInfluencesInput = EPCGExInputValueType::Constant;

	/** Max influences Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Other", meta=(PCG_Overridable, DisplayName="Max Influences (Attr)", EditCondition="MaxInfluencesInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxInfluencesAttribute = FName("MaxInfluences");

	/** Max influences Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Other", meta=(PCG_Overridable, DisplayName="Max Influences", EditCondition="MaxInfluencesInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 MaxInfluences = 1;

#pragma endregion

#pragma region  Outputs

	/** Write the diffusion depth the vtx was subjected to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionDepth = false;

	/** Name of the 'int32' attribute to write diffusion depth to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Depth", PCG_Overridable, EditCondition="bWriteDiffusionDepth"))
	FName DiffusionDepthAttributeName = FName("DiffusionDepth");

	/** Write the final diffusion order. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionOrder = false;

	/** Name of the 'int32' attribute to write order to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Order", PCG_Overridable, EditCondition="bWriteDiffusionOrder"))
	FName DiffusionOrderAttributeName = FName("DiffusionOrder");

	/** Write the final diffusion distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionDistance = false;

	/** Name of the 'double' attribute to write diffusion distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Distance", PCG_Overridable, EditCondition="bWriteDiffusionDistance"))
	FName DiffusionDistanceAttributeName = FName("DiffusionDistance");

#pragma endregion

	/** Which Seed attributes to forward on the vtx they diffused to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="Seeds==EPCGExDiffusionSeeds::Points"))
	FPCGExForwardDetails SeedForwarding;

	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExClusterDiffusionElement;
};

struct FPCGExClusterDiffusionContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExClusterDiffusionElement;

	TArray<TObjectPtr<const UPCGExAttributeBlendFactory>> BlendingFactories;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;
	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;

	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL_TOGGLE)
};

class FPCGExClusterDiffusionElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExClusterDiffusion
{
	class FBatch;
	class FProcessor;

	struct FCandidate
	{
		const PCGExCluster::FNode* Node = nullptr;
		int32 Depth = 0;
		double Score = 0;
		double Distance = 0;

		FCandidate() = default;
	};

	class FDiffusion : public TSharedFromThis<FDiffusion>
	{
		friend class FProcessor;

	protected:
		TArray<FCandidate> Candidates;
		TArray<FCandidate> Staged;
		TArray<FCandidate> Captured;

		TSet<int32> Visited;
		TSharedPtr<PCGEx::FMapHashLookup> TravelStack; // Required for heuristics
		// use map hash lookup to reduce memory overhead of a shared map + thread safety yay

		TSharedPtr<FProcessor> Processor;
		TSharedPtr<PCGExCluster::FCluster> Cluster;

		int32 MaxDepth = 0;
		double MaxDistance = 0;

	public:
		bool bStopped = false;
		const PCGExCluster::FNode* SeedNode = nullptr;
		int32 SeedIndex = -1;
		int32 DiffusionRate = 1;

		int32 CountLimit = MAX_int32;
		int32 DepthLimit = MAX_int32;
		double DistanceLimit = MAX_dbl;

		FDiffusion(const TSharedPtr<FProcessor>& InProcessor, const PCGExCluster::FNode* InSeedNode);
		~FDiffusion() = default;

		void Init();
		void Probe(const FCandidate& From);
		void Grow();
		void PostGrow();

		void Diffuse();
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterDiffusionContext, UPCGExClusterDiffusionSettings>
	{
		friend FBatch;
		friend FDiffusion;

	protected:
		const PCGExCluster::FNode* RoamingSeedNode = nullptr;
		const PCGExCluster::FNode* RoamingGoalNode = nullptr;

		TSharedPtr<TArray<int32>> InfluencesCount;
		TSharedPtr<TArray<UPCGExAttributeBlendOperation*>> Operations;

		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<FDiffusion>>> InitialDiffusions;
		TArray<TSharedPtr<FDiffusion>> OngoingDiffusions; // Ongoing diffusions
		TArray<TSharedPtr<FDiffusion>> Diffusions;        // Stopped diffusions, as to not iterate over them needlessly
		
		TSharedPtr<PCGExData::TBuffer<int32>> DiffusionRate;
		TSharedPtr<PCGExData::TBuffer<int32>> CountLimit;
		TSharedPtr<PCGExData::TBuffer<int32>> DepthLimit;
		TSharedPtr<PCGExData::TBuffer<double>> DistanceLimit;

		TSharedPtr<PCGExMT::TScopedValue<double>> MaxDistanceValue;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			//bDaisyChainProcessRange = true; // TODO : Evaluate atomic operations for influence update, it's the only contention at the moment
		}

		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		void StartGrowth();
		void Grow();

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatchWithHeuristics<FProcessor>
	{
		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

	protected:
		TSharedPtr<TArray<int32>> InfluencesCount;
		TSharedPtr<TArray<UPCGExAttributeBlendOperation*>> Operations;

		TSharedPtr<PCGExData::TBuffer<int32>> DiffusionRate;
		TSharedPtr<PCGExData::TBuffer<int32>> CountLimit;
		TSharedPtr<PCGExData::TBuffer<int32>> DepthLimit;
		TSharedPtr<PCGExData::TBuffer<double>> DistanceLimit;
		
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void Write() override;
	};
}
