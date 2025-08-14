// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "Transform/PCGExFitting.h"
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
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** If enabled, allows you to pick which input gets copied to which target point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching;

	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails = FPCGExTransformDetails(true, true);

	/** TBD */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding")
	FPCGExAttributeToTagDetails TargetsAttributesToCopyTags;

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

	FPCGExAttributeToTagDetails TargetsAttributesToCopyTags;
	TSharedPtr<PCGExData::FDataForwardHandler> TargetsForwardHandler;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
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
		PCGExMatching::FMatchingScope MatchScope;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
