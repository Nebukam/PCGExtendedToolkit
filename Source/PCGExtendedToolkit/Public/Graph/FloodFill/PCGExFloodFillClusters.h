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
#include "Graph/FloodFill/PCGExFloodFill.h"
#include "PCGExFloodFillClusters.generated.h"

#define PCGEX_FOREACH_FIELD_CLUSTER_DIFF(MACRO)\
MACRO(DiffusionDepth, int32, -1)\
MACRO(DiffusionOrder, int32, -1)\
MACRO(DiffusionDistance, double, 0)\
MACRO(DiffusionEnding, bool, false)

UENUM()
enum class EPCGExFloodFillOrder : uint8
{
	Index   = 0 UMETA(DisplayName = "Index", ToolTip="Uses point index to drive diffusion order."),
	Sorting = 1 UMETA(DisplayName = "Sorting", ToolTip="Use sorting rules to drive diffusion order."),
};

UENUM()
enum class EPCGExFloodFillProcessing : uint8
{
	Parallel = 0 UMETA(DisplayName = "Parallel", ToolTip="Diffuse each vtx once before moving to the next iteration."),
	Sequence = 1 UMETA(DisplayName = "Sequential", ToolTip="Diffuse each vtx until it stops before moving to the next one, and so on."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFloodFillSeedPickingDetails
{
	GENERATED_BODY()

	FPCGExFloodFillSeedPickingDetails()
	{
	}

	/** Drive how a seed point selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);

	/** Defines the sorting used for the vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Source==EPCGExFloodFillSource::Filters", EditConditionHides))
	EPCGExFloodFillOrder Ordering = EPCGExFloodFillOrder::Index;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Source==EPCGExFloodFillSource::Filters", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
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
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Seeds settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExFloodFillSeedPickingDetails Seeds;

	/** Defines how each vtx is diffused */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFloodFillProcessing Processing = EPCGExFloodFillProcessing::Parallel;

	/** Diffusion settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExFloodFillFlowDetails Diffusion;

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

	/** Write on the vtx whether it's a diffusion "endpoint". */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionEnding = false;

	/** Name of the 'bool' attribute to write diffusion ending to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Ending", PCG_Overridable, EditCondition="bWriteDiffusionEnding"))
	FName DiffusionEndingAttributeName = FName("DiffusionEnding");

#pragma endregion

	/** Which Seed attributes to forward on the vtx they diffused to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable, EditCondition="Seeds==EPCGExFloodFillSource::Points"))
	FPCGExForwardDetails SeedForwarding = FPCGExForwardDetails(true);


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs - Paths")
	bool bOutputPaths = false;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs - Paths", meta=(EditCondition="bOutputPaths"))
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;


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
	TArray<TObjectPtr<const UPCGExFillControlsFactoryData>> FillControlFactories;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;

	TSharedPtr<PCGExData::FPointIOCollection> Paths;

	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL_TOGGLE)

	int32 ExpectedPathCount = 0;
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

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterDiffusionContext, UPCGExClusterDiffusionSettings>
	{
		friend FBatch;

	protected:
		const PCGExCluster::FNode* RoamingSeedNode = nullptr;
		const PCGExCluster::FNode* RoamingGoalNode = nullptr;

		TSharedPtr<TArray<int8>> InfluencesCount;
		TArray<int8> Seeded;
		TSharedPtr<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>> Operations;

		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<PCGExFloodFill::FDiffusion>>> InitialDiffusions;
		TArray<TSharedPtr<PCGExFloodFill::FDiffusion>> OngoingDiffusions; // Ongoing diffusions
		TArray<TSharedPtr<PCGExFloodFill::FDiffusion>> Diffusions;        // Stopped diffusions, as to not iterate over them needlessly

		TSharedPtr<PCGExFloodFill::FFillControlsHandler> FillControlsHandler;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> FillRate;

		TSharedPtr<PCGExMT::TScopedNumericValue<double>> MaxDistanceValue;

		int32 ExpectedPathCount = 0;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bAllowEdgesDataFacadeScopedGet = false;
		}

		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		void StartGrowth();
		void Grow();

		virtual void ProcessSingleRangeIteration(const int32 Iteration, const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void CompleteWork() override;
		void Diffuse(const TSharedPtr<PCGExFloodFill::FDiffusion>& Diffusion);

		virtual void Output() override;
		void WritePath(const int32 DiffusionIndex, const int32 EndpointNodeIndex);

		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExClusterMT::TBatchWithHeuristics<FProcessor>
	{
		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

	protected:
		TSharedPtr<TArray<int8>> InfluencesCount;
		TSharedPtr<TArray<TSharedPtr<FPCGExAttributeBlendOperation>>> BlendOps;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> FillRate;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void Write() override;
	};
}
