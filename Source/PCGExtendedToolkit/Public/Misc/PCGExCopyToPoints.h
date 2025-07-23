// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"
#include "Data/PCGExFilterGroup.h"
#include "Data/Matching/PCGExMatching.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"

#include "PCGExCopyToPoints.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/copy-to-points"))
class UPCGExCopyToPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CopyToPoints, "Copy to Points", "Copy source points to target points, with size-to-fit and justification goodies.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite); }
	//PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, true)
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails = FPCGExTransformDetails(true, true);

	/** If enabled, allows you to pick which input gets copied to which target point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoMatchByTags = false;

	/** Use tag to filter which cluster gets copied to which target point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoMatchByTags"))
	FPCGExAttributeToTagComparisonDetails MatchByTagValue;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoMatchByData = false;

	/** Use @Data to filter which cluster gets copied to which target point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoMatchByData"))
	FPCGExAttributeToDataComparisonDetails MatchByDataValue;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoMatchByTags && bDoMatchByData"))
	EPCGExFilterGroupMode Mode = EPCGExFilterGroupMode::AND;

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails TargetsAttributesToClusterTags;

	/** Which target attributes to forward on copied points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExForwardDetails TargetsForwarding;
};

struct FPCGExCopyToPointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExCopyToPointsElement;
	friend class UPCGExCopyToPointsSettings;

	FPCGExTransformDetails TransformDetails;

	TSharedPtr<PCGExData::FFacade> TargetsDataFacade;

	TSharedPtr<PCGExMatching::FDataMatcher> DataMatcher;

	FPCGExAttributeToTagComparisonDetails MatchByTagValue;
	FPCGExAttributeToDataComparisonDetails MatchByDataValue;
	FPCGExAttributeToTagDetails TargetsAttributesToClusterTags;
	TSharedPtr<PCGExData::FDataForwardHandler> TargetsForwardHandler;
};

class FPCGExCopyToPointsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CopyToPoints)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExCopyToPoints
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCopyToPointsContext, UPCGExCopyToPointsSettings>
	{
	protected:
		TArray<TSharedPtr<PCGExData::FPointIO>> Dupes;
		int32 NumCopies = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
	};
}
