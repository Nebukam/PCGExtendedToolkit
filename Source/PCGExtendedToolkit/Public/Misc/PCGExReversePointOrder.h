// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"


#include "PCGExReversePointOrder.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSwapAttributePairDetails
{
	GENERATED_BODY()

	FPCGExSwapAttributePairDetails()
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName FirstAttributeName = NAME_None;
	PCGEx::FAttributeIdentity* FirstIdentity = nullptr;
	TSharedPtr<PCGExData::FBufferBase> FirstWriter;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName SecondAttributeName = NAME_None;
	PCGEx::FAttributeIdentity* SecondIdentity = nullptr;
	TSharedPtr<PCGExData::FBufferBase> SecondWriter;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMultiplyByMinusOne = false;

	bool Validate(const FPCGContext* InContext) const
	{
		PCGEX_VALIDATE_NAME_C(InContext, FirstAttributeName)
		PCGEX_VALIDATE_NAME_C(InContext, SecondAttributeName)
		return true;
	}
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExReversePointOrderSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ReversePointOrder, "Reverse Point Order", "Simply reverse the order of points. Very useful with paths.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscWrite; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSwapAttributePairDetails> SwapAttributesValues;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExReversePointOrderContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExReversePointOrderElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExReversePointOrderElement final : public FPCGExPointsProcessorElement
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

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExReversePointOrderTask final : public PCGExMT::FPCGExTask
{
public:
	FPCGExReversePointOrderTask(const TSharedPtr<PCGExData::FPointIO>& InPointIO) :
		FPCGExTask(InPointIO)
	{
	}

	virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
};

namespace PCGExReversePointOrder
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExReversePointOrderContext, UPCGExReversePointOrderSettings>
	{
		TArray<FPCGExSwapAttributePairDetails> SwapPairs;
		TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}
