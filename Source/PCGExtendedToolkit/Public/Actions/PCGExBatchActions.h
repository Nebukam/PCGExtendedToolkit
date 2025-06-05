// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"


#include "PCGExBatchActions.generated.h"

class FPCGExActionOperation;
class UPCGExActionFactoryData;
/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExBatchActionsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BatchActions, "Batch Actions", "Batch various actions together.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
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
	friend class FPCGExBatchActionsElement;
};

struct FPCGExBatchActionsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBatchActionsElement;

	TSharedPtr<PCGEx::FAttributesInfos> DefaultAttributes;
	TArray<TObjectPtr<const UPCGExActionFactoryData>> ActionsFactories;
};

class FPCGExBatchActionsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BatchActions)
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExBatchActions
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExBatchActionsContext, UPCGExBatchActionsSettings>
	{
		TArray<TSharedPtr<FPCGExActionOperation>> Operations;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Cleanup() override;
	};
}
