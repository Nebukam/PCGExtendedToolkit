// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Containers/PCGExScopedContainers.h"

#include "Core/PCGExClustersProcessor.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "Elements/FloodFill/PCGExFloodFill.h"
#include "Sampling/PCGExSamplingCommon.h"
#include "PCGExFloodFillClusters.generated.h"

#define PCGEX_FOREACH_FIELD_CLUSTER_DIFF(MACRO)\
MACRO(DiffusionDepth, int32, -1)\
MACRO(DiffusionOrder, int32, -1)\
MACRO(DiffusionDistance, double, 0)\
MACRO(DiffusionEnding, bool, false)

class UPCGExBlendOpFactory;

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

UENUM()
enum class EPCGExFloodFillPathOutput : uint8
{
	None       = 0 UMETA(DisplayName = "None", ToolTip="Don't output any paths."),
	Full       = 1 UMETA(DisplayName = "Full", ToolTip="Output full paths, from seed to end point -- generate a lot of overlap."),
	Partitions = 2 UMETA(DisplayName = "Partitions", ToolTip="Output partial paths, only endpoints will overlap. "),
};

UENUM()
enum class EPCGExFloodFillPathPartitions : uint8
{
	Length = 0 UMETA(DisplayName = "Length", ToolTip="TBD"),
	Score  = 1 UMETA(DisplayName = "Score", ToolTip="TBD"),
	Depth  = 2 UMETA(DisplayName = "Depth", ToolTip="TBD"),
};

USTRUCT(BlueprintType)
struct PCGEXELEMENTSCLUSTERS_API FPCGExFloodFillSeedPickingDetails
{
	GENERATED_BODY()

	FPCGExFloodFillSeedPickingDetails()
	{
	}

	/** Drive how a seed point selects a node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExNodeSelectionDetails SeedPicking = FPCGExNodeSelectionDetails(200);

	/** Defines the sorting used for the vtx */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFloodFillOrder Ordering = EPCGExFloodFillOrder::Index;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/flood-fill"))
class UPCGExClusterDiffusionSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	UPCGExClusterDiffusionSettings(const FObjectInitializer& ObjectInitializer);

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	FPCGExForwardDetails SeedForwarding = FPCGExForwardDetails(true);


	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs - Paths")
	EPCGExFloodFillPathOutput PathOutput = EPCGExFloodFillPathOutput::None;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs - Paths", meta=(DisplayName=" ├─ Partition over", EditCondition="PathOutput == EPCGExFloodFillPathOutput::Partitions"))
	EPCGExFloodFillPathPartitions PathPartitions = EPCGExFloodFillPathPartitions::Length;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs - Paths", meta=(DisplayName=" └─ Sorting", EditCondition="PathOutput == EPCGExFloodFillPathOutput::Partitions"))
	EPCGExSortDirection PartitionSorting = EPCGExSortDirection::Ascending;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs - Paths", meta=(EditCondition="PathOutput != EPCGExFloodFillPathOutput::None"))
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;


	/** Whether or not to search for closest node using an octree. Depending on your dataset, enabling this may be either much faster, or much slower. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bUseOctreeSearch = false;

private:
	friend class FPCGExClusterDiffusionElement;
};

struct FPCGExClusterDiffusionContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExClusterDiffusionElement;

	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;
	TArray<TObjectPtr<const UPCGExFillControlsFactoryData>> FillControlFactories;

	TSharedPtr<PCGExData::FFacade> SeedsDataFacade;
	FPCGExAttributeToTagDetails SeedAttributesToPathTags;
	TSharedPtr<PCGExData::FDataForwardHandler> SeedForwardHandler;

	TSharedPtr<PCGExData::FPointIOCollection> Paths;

	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL_TOGGLE)

	int32 ExpectedPathCount = 0;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExClusterDiffusionElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ClusterDiffusion)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExClusterDiffusion
{
	class FBatch;
	class FProcessor;

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterDiffusionContext, UPCGExClusterDiffusionSettings>
	{
		friend FBatch;

	protected:
		const PCGExClusters::FNode* RoamingSeedNode = nullptr;
		const PCGExClusters::FNode* RoamingGoalNode = nullptr;

		TSharedPtr<TArray<int8>> InfluencesCount;
		TArray<int8> Seeded;
		TSharedPtr<PCGExBlending::FBlendOpsManager> BlendOpsManager;

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

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		void StartGrowth();
		void Grow();

		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void CompleteWork() override;
		void Diffuse(const TSharedPtr<PCGExFloodFill::FDiffusion>& Diffusion);

		void OnDiffusionComplete();
		void WriteFullPath(const int32 DiffusionIndex, const int32 EndpointNodeIndex);
		void WritePath(const int32 DiffusionIndex, TArray<int32>& PathIndices);

		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

	protected:
		TSharedPtr<TArray<int8>> InfluencesCount;
		TSharedPtr<PCGExBlending::FBlendOpsManager> BlendOpsManager;
		TSharedPtr<PCGExDetails::TSettingValue<int32>> FillRate;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor) override;
		virtual void Write() override;
	};
}
