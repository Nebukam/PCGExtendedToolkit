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
MACRO(DiffusionDistance, double, 0)

UENUM()
enum class EPCGExDiffusionOrder : uint8
{
	Index   = 0 UMETA(DisplayName = "Index", ToolTip="Uses point index to drive diffusion order."),
	Sorting = 1 UMETA(DisplayName = "Sorting", ToolTip="Use sorting rules to drive diffusion order."),
};

UENUM()
enum class EPCGExDiffusionSeeds : uint8
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
	/** Defines how each vtx is diffused */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDiffusionProcessing Processing = EPCGExDiffusionProcessing::Parallel;

	/** Defines the type of seeds used for diffusion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExDiffusionSeeds Seeds = EPCGExDiffusionSeeds::Points;

	/** Drive how a seed selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Seeds==EPCGExDiffusionSeeds::Points", EditConditionHides))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);
	
	/** Defines the sorting used for the vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Seeds==EPCGExDiffusionSeeds::Filters", EditConditionHides))
	EPCGExDiffusionOrder Ordering = EPCGExDiffusionOrder::Index;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Seeds==EPCGExDiffusionSeeds::Filters", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

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

	// Notes
	// - Max Range mode
	//		- Count
	//		- Distance
	//		- Radius
	// - Stop conditions (optional)
	// - Priority management (which vtx overrides which)
	//		- Fallback to next best candidate, if available, otherwise stop "growth"
	//		- Rely on this to sort diffusions and process capture in order
	//		- 
	//
	// - First compute growth map with reverse parent lookup
	//		- Maintain list of active candidates "layer"
	//			- Find direct neighbors
	//			- Sort them by depth then score? depth only? score only? -> Need sort mode
	//			- Since there is no goal, using heuristics may prove tricky
	//				- Might need new detail to override roaming coordinates per-vtx (should be useful elsewhere, i.e cluster refine)

	// Many formal steps required
	// - Build layers and growth, sorting based on score (can be parallelized)
	// - Capture node (must be inlined, can use large batches)
	// - Once all growths have been stopped, blending can happen

#pragma region  Outputs

	/** Write the diffusion depth the vtx was subjected to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionDepth = false;

	/** Name of the 'int32' attribute to write diffusion depth to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Depth", PCG_Overridable, EditCondition="bWriteDiffusionDepth"))
	FName DiffusionDepthAttributeName = FName("DiffusionDepth");

	/** Write the final diffusion distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionDistance = false;

	/** Name of the 'double' attribute to write diffusion distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Distance", PCG_Overridable, EditCondition="bWriteDiffusionDistance"))
	FName DiffusionDistanceAttributeName = FName("DiffusionDistance");

#pragma endregion

	/** Which Seed attributes to forward on the vtx they diffused to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Seeds==EPCGExDiffusionSeeds::Points"))
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
		TArray<FCandidate> Captured;
		TSet<int32> Visited;
		TSharedPtr<PCGEx::FMapHashLookup> TravelStack; // Required for heuristics
		// use map hash lookup to reduce memory overhead of a shared map + thread safety yay

		TSharedPtr<FProcessor> Processor;

	public:
		bool bStopped = false;
		TSharedPtr<PCGExCluster::FCluster> Cluster;
		const PCGExCluster::FNode* SeedNode = nullptr;
		int32 SeedIndex = -1;

		FDiffusion(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const PCGExCluster::FNode* InSeedNode);
		~FDiffusion() = default;

		void Init();

		void Probe(const FCandidate& From);
		void Grow();
		void SortCandidates();
		void Diffuse();
		void Complete(FPCGExClusterDiffusionContext* Context, const TSharedPtr<PCGExData::FFacade>& InVtxFacade);
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

		TSharedPtr<PCGExMT::TScopedValue<double>> MaxDistanceValue;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bDaisyChainProcessRange = true;
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
		
	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void Write() override;
	};
}
