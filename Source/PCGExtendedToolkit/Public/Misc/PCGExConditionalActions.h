// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"


#include "PCGExConditionalActions.generated.h"

class UPCGExConditionalActionOperation;
class UPCGExConditionalActionFactoryBase;
/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConditionalActions, "Conditional Actions", "TBD.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails DefaultAttributesFilter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bDoConsumeProcessedAttributes = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bDoConsumeProcessedAttributes"))
	FPCGExNameFiltersDetails ConsumeProcessedAttributes;

private:
	friend class FPCGExConditionalActionsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExConditionalActionsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExConditionalActionsElement;

	TSharedPtr<PCGEx::FAttributesInfos> DefaultAttributes;
	TArray<TObjectPtr<const UPCGExConditionalActionFactoryBase>> ConditionalActionsFactories;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExConditionalActionsElement final : public FPCGExPointsProcessorElement
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

namespace PCGExConditionalActions
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExConditionalActionsContext, UPCGExConditionalActionsSettings>
	{
		TArray<UPCGExConditionalActionOperation*> Operations;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
