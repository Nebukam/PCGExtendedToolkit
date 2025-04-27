// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExScopedContainers.h"
#include "Data/PCGExDataForward.h"
#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExFloodFillClusters.generated.h"

#define PCGEX_FOREACH_FIELD_CLUSTER_DIFF(MACRO)\
MACRO(DiffusionDepth, int32, -1)\
MACRO(DiffusionOrder, int32, -1)\
MACRO(DiffusionDistance, double, 0)

UENUM()
enum class EPCGExFloodFillOrder : uint8
{
	Index   = 0 UMETA(DisplayName = "Index", ToolTip="Uses point index to drive diffusion order."),
	Sorting = 1 UMETA(DisplayName = "Sorting", ToolTip="Use sorting rules to drive diffusion order."),
};

UENUM()
enum class EPCGExFloodFillSource : uint8
{
	Filters = 0 UMETA(DisplayName = "Filters", ToolTip="Uses filters to pick which vtx should be used as seeds."),
	Points  = 1 UMETA(DisplayName = "Points", ToolTip="Uses source seed points as seed, and picks the closest vtx."),
};

UENUM()
enum class EPCGExFloodFillProcessing : uint8
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

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Diffusion Heuristic Flags"))
enum class EPCGExDiffusionHeuristicFlags : uint8
{
	None          = 0,
	LocalScore    = 1 << 0 UMETA(DisplayName = "Local Score", ToolTip="From neighbor to neighbor"),
	GlobalScore   = 1 << 1 UMETA(DisplayName = "Global Score", ToolTip="From seed to candidate"),
	PreviousScore = 1 << 2 UMETA(DisplayName = "Previous Score", ToolTip="Previously accumulated local score"),
};

ENUM_CLASS_FLAGS(EPCGExDiffusionHeuristicFlags)
using EPCGExDiffusionHeuristicFlagsBitmask = TEnumAsByte<EPCGExDiffusionHeuristicFlags>;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFloodFillSeedPickingDetails
{
	GENERATED_BODY()

	FPCGExFloodFillSeedPickingDetails()
	{
	}

	/** Defines the type of seeds used for diffusion. Points lets you use a single point dataset as spatial input, while filters will use filters on the vtx to determine which vtx should be diffused. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFloodFillSource Source = EPCGExFloodFillSource::Points;

	/** Drive how a seed point selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Source==EPCGExFloodFillSource::Points", EditConditionHides))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);

	/** Defines the sorting used for the vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Source==EPCGExFloodFillSource::Filters", EditConditionHides))
	EPCGExFloodFillOrder Ordering = EPCGExFloodFillOrder::Index;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Source==EPCGExFloodFillSource::Filters", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFloodFillFlowDetails
{
	GENERATED_BODY()

	FPCGExFloodFillFlowDetails()
	{
	}

	/** Defines how each vtx is diffused */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFloodFillProcessing Processing = EPCGExFloodFillProcessing::Parallel;

	/** Which data should be prioritized to 'drive' diffusion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDiffusionPrioritization Priority = EPCGExDiffusionPrioritization::Heuristics;

	/** Diffusion Rate type.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Processing==EPCGExFloodFillProcessing::Parallel", EditConditionHides))
	EPCGExInputValueType FillRateInput = EPCGExInputValueType::Constant;

	/** Fetch the Diffusion Rate from a local attribute. Must be >= 0, but zero wont grow -- it will however "preserve" the vtx from being diffused on. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Fill Rate (Attr)", EditCondition="Processing==EPCGExFloodFillProcessing::Parallel && FillRateInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName FillRateAttribute = FName("FillRate");

	/** Diffusion rate constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Fill Rate", EditCondition="Processing==EPCGExFloodFillProcessing::Parallel && FillRateInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	int32 FillRateConstant = 1;

	/** What components are used for scoring points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExDiffusionHeuristicFlags"))
	uint8 Scoring = static_cast<uint8>(EPCGExDiffusionHeuristicFlags::LocalScore);
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
	PCGEX_NODE_INFOS(ClusterFloodFill, "Cluster : Flood Fill", "Diffuses vtx attributes onto their neighbors.");
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
	FPCGExFloodFillSeedPickingDetails Seeds;

	/** Diffusion settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExFloodFillFlowDetails Diffusion;

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
	int32 MaxCount = 10;

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

	// Keep within running average

	/** Whether to limit the expansion to heuristic values within a tolerance of running average score */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Running Average", meta=(PCG_NotOverridable))
	bool bUseRunningAverage = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Running Average", meta=(PCG_NotOverridable, EditCondition="bUseRunningAverage", EditConditionHides))
	EPCGExInputValueType RunningAverageInput = EPCGExInputValueType::Constant;

	/** Max radius Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Running Average", meta=(PCG_Overridable, DisplayName="Running Average (Attr)", EditCondition="bUseRunningAverage && RunningAverageInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName RunningAverageAttribute = FName("MaxRadius");

	/** Max radius Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Limits|Running Average", meta=(PCG_Overridable, DisplayName="Running Average", EditCondition="bUseRunningAverage && RunningAverageInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0))
	double RunningAverage = 100;
	
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="Seeds==EPCGExFloodFillSource::Points"))
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
		double PathScore = 0;
		double Score = 0;
		double Distance = 0;

		FCandidate() = default;
	};

	class FDiffusion : public TSharedFromThis<FDiffusion>
	{
		friend class FProcessor;

	protected:
		TArray<FCandidate> Candidates;
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
		int32 FillRate = 1;

		int32 CountLimit = MAX_int32;
		int32 DepthLimit = MAX_int32;
		double DistanceLimit = MAX_dbl;
		double RunningAverageLimit = MAX_dbl;

		double ScoreSum = 0;
		
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

		TSharedPtr<TArray<int8>> InfluencesCount;
		TSharedPtr<TArray<UPCGExAttributeBlendOperation*>> Operations;

		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<FDiffusion>>> InitialDiffusions;
		TArray<TSharedPtr<FDiffusion>> OngoingDiffusions; // Ongoing diffusions
		TArray<TSharedPtr<FDiffusion>> Diffusions;        // Stopped diffusions, as to not iterate over them needlessly

		TSharedPtr<PCGExDetails::TSettingValue<int32>> FillRate;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> CountLimit;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> DepthLimit;
		TSharedPtr<PCGExDetails::TSettingValue<double>> DistanceLimit;
		TSharedPtr<PCGExDetails::TSettingValue<double>> RunningAverageLimit;

		TSharedPtr<PCGExMT::TScopedValue<double>> MaxDistanceValue;

		bool bUseLocalScore = false;
		bool bUseGlobalScore = false;
		bool bUsePreviousScore = false;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
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
		TSharedPtr<TArray<int8>> InfluencesCount;
		TSharedPtr<TArray<UPCGExAttributeBlendOperation*>> Operations;

		TSharedPtr<PCGExDetails::TSettingValue<int32>> FillRate;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> CountLimit;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> DepthLimit;
		TSharedPtr<PCGExDetails::TSettingValue<double>> DistanceLimit;
		TSharedPtr<PCGExDetails::TSettingValue<double>> RunningAverageLimit;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void Write() override;
	};
}
