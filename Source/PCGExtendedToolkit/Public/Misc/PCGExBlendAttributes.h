// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"


#include "PCGExBlendAttributes.generated.h"

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class UPCGExBlendAttributesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BlendAttributes, "Uber Blend", "[EXPERIMENTAL] One-stop node to combine multiple blends.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Metadata; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite); }
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Whther to write the index as a normalized output value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOutputNormalizedIndex = false;
};

struct FPCGExBlendAttributesContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBlendAttributesElement;

	TArray<TObjectPtr<const UPCGExAttributeBlendFactory>> BlendingFactories;
};

class FPCGExBlendAttributesElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBlendAttributes
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBlendAttributesContext, UPCGExBlendAttributesSettings>
	{
		double NumPoints = 0;
		TSharedPtr<PCGExDataBlending::FBlendOpsManager> BlendOpsManager;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		void BlendScope(const PCGExMT::FScope& InScope);
		virtual void CompleteWork() override;
		virtual void Cleanup() override;
	};
}
