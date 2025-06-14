// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExScopedContainers.h"


#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Graph/Filters/PCGExClusterFilter.h"

#include "PCGExFilterVtx.generated.h"

UENUM()
enum class EPCGExVtxFilterOutput : uint8
{
	Clusters  = 0 UMETA(DisplayName = "Clusters", ToolTip="Outputs clusters."),
	Points    = 1 UMETA(DisplayName = "Points", ToolTip="Outputs regular points"),
	Attribute = 3 UMETA(DisplayName = "Attribute", ToolTip="Writes the result of the filters to a boolean attribute."),
};

namespace PCGExFilterVtx
{
	const FName SourceSanitizeEdgeFilters = FName("SanitizeFilters");
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/find-clusters-data-1"))
class UPCGExFilterVtxSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FilterVtx, "Cluster : Filter Vtx", "Filter out vtx from clusters.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorEdge); }
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

	/** Type of output */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExVtxFilterOutput Mode = EPCGExVtxFilterOutput::Clusters;

	/** Invert the filter result */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	/** Invert the edge filters result */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode == EPCGExVtxFilterOutput::Clusters", EditConditionHides))
	bool bInvertEdgeFilters = false;

	/** Name of the attribute to write the filter result to. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Mode == EPCGExVtxFilterOutput::Attribute", EditConditionHides))
	FName ResultAttributeName = FName("PassFilters");

	/** If enabled, inside/outside groups will be partitioned by initial edge connectivity. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="Mode == EPCGExVtxFilterOutput::Points", EditConditionHides))
	bool bSplitOutputsByConnectivity = true;

	/** Swap Inside & Outside content */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="Mode == EPCGExVtxFilterOutput::Points", EditConditionHides))
	bool bSwap = false;

	/** */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfAnyPointPassed = false;

	/** ... */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfAnyPointPassed"))
	FString HasAnyPointPassedTag = TEXT("SomePointsPassed");

	/** */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfAllPointsPassed = false;

	/** ... */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfAllPointsPassed"))
	FString AllPointsPassedTag = TEXT("AllPointsPassed");

	/** */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfNoPointPassed = false;

	/** ... */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfNoPointPassed"))
	FString NoPointPassedTag = TEXT("NoPointPassed");


	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings", EditCondition="Mode == EPCGExVtxFilterOutput::Clusters", EditConditionHides))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

private:
	friend class FPCGExFilterVtxElement;
};

struct FPCGExFilterVtxContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExFilterVtxElement;

	bool bWantsClusters = true;

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> VtxFilterFactories;
	TArray<TObjectPtr<const UPCGExFilterFactoryData>> EdgeFilterFactories;

	TSharedPtr<PCGExData::FPointIOCollection> Inside;
	TSharedPtr<PCGExData::FPointIOCollection> Outside;
};

class FPCGExFilterVtxElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(FilterVtx)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExFilterVtx
{
	const FName SourceOverridesRefinement = TEXT("Overrides : Refinement");

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExFilterVtxContext, UPCGExFilterVtxSettings>
	{
		friend class FBatch;

	protected:
		TSharedPtr<PCGExData::TBuffer<bool>> TestResults;
		TSharedPtr<PCGExMT::TScopedNumericValue<int32>> ScopedPassNum;
		int32 PassNum = 0;

		TSharedPtr<PCGExMT::TScopedNumericValue<int32>> ScopedFailNum;
		int32 FailNum = 0;

		virtual TSharedPtr<PCGExCluster::FCluster> HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef) override;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;

		virtual void PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessNodes(const PCGExMT::FScope& Scope) override;

		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;

		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<bool>> TestResults;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			PCGEX_TYPED_CONTEXT_AND_SETTINGS(FilterVtx)

			bRequiresGraphBuilder = Settings->Mode == EPCGExVtxFilterOutput::Clusters;
			bRequiresWriteStep = Settings->Mode == EPCGExVtxFilterOutput::Attribute;
		}

		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}
