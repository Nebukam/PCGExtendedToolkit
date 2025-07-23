// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExCompare.h"
#include "PCGExEdgesProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Data/Matching/PCGExMatching.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"


#include "PCGExCopyClustersToPoints.generated.h"

UENUM()
enum class EPCGExClusterComponentTagMatchMode : uint8
{
	Vtx   = 0 UMETA(DisplayName = "Vtx", ToolTip="Only match vtx (most efficient check)"),
	Edges = 1 UMETA(DisplayName = "Edges", ToolTip="Only match edges"),
	Any   = 2 UMETA(DisplayName = "Any", ToolTip="Match either vtx or edges"),
	Both  = 3 UMETA(DisplayName = "Vtx and Edges", ToolTip="Match no vtx and edges"),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/copy-clusters-to-points"))
class UPCGExCopyClustersToPointsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CopyClustersToPoints, "Cluster : Copy to Points", "Create a copies of the input clusters onto the target points.  NOTE: Does not sanitize input.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorCluster; }
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails;

	/** If enabled, allows you to pick which input gets copied to which target point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoMatchByTags = false;

	/** Which cluster component must match the tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoMatchByTags"))
	EPCGExClusterComponentTagMatchMode MatchMode = EPCGExClusterComponentTagMatchMode::Vtx;

	/** Use tag to filter which cluster gets copied to which target point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoMatchByTags", HideEditConditionToggle))
	FPCGExAttributeToTagComparisonDetails MatchByTagValue;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails TargetsAttributesToClusterTags;

	/** Which target attributes to forward on clusters. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails TargetsForwarding;
};

struct FPCGExCopyClustersToPointsContext final : FPCGExEdgesProcessorContext
{
	friend class UPCGExCopyClustersToPointsSettings;
	friend class FPCGExCopyClustersToPointsElement;

	FPCGExTransformDetails TransformDetails;

	TSharedPtr<PCGExData::FFacade> TargetsDataFacade;
	TSharedPtr<PCGExMatching::FDataMatcher> DataMatcher;

	FPCGExAttributeToTagComparisonDetails MatchByTagValue;
	FPCGExAttributeToTagDetails TargetsAttributesToClusterTags;
	TSharedPtr<PCGExData::FDataForwardHandler> TargetsForwardHandler;
};

class FPCGExCopyClustersToPointsElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CopyClustersToPoints)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExCopyClusters
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExCopyClustersToPointsContext, UPCGExCopyClustersToPointsSettings>
	{
		friend class FBatch;

	protected:
		int32 NumCopies = 0;

	public:
		TArray<TSharedPtr<PCGExData::FPointIO>>* VtxDupes = nullptr;
		TArray<PCGExData::DataIDType>* VtxTag = nullptr;

		TArray<TSharedPtr<PCGExData::FPointIO>> EdgesDupes;

		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade):
			TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
			bBuildCluster = false;
		}

		virtual ~FProcessor() override;
		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void CompleteWork() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	protected:
		int32 NumCopies = 0;

	public:
		TArray<TSharedPtr<PCGExData::FPointIO>> VtxDupes;
		TArray<PCGExData::DataIDType> VtxTag;

		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
		}

		virtual ~FBatch() override;
		virtual void Process() override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void CompleteWork() override;
	};
}
