// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExUberFilter.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Uber Filter Mode"))
enum class EPCGExUberFilterMode : uint8
{
	Partition UMETA(DisplayName = "Partition points", ToolTip="Create inside/outside dataset from the filter results."),
	Write UMETA(DisplayName = "Write result", ToolTip="Simply write filter result to an attribute but doesn't change point structure."),
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExUberFilterSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
#if WITH_EDITOR

public:
#endif
	//~End UObject interface

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(UberFilter, "Uber Filter", "Filter points based on multiple rules & conditions.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorFilterHub; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Write result to point instead of split outputs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExUberFilterMode Mode = EPCGExUberFilterMode::Partition;

	/** Name of the attribute to write result to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExUberFilterMode::Write", EditConditionHides))
	FName ResultAttributeName = FName("PassFilter");

	/** Invert the filter result */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSwap = false;

private:
	friend class FPCGExUberFilterElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExUberFilterContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExUberFilterElement;
	virtual ~FPCGExUberFilterContext() override;

	PCGExData::FPointIOCollection* Inside = nullptr;
	PCGExData::FPointIOCollection* Outside = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExUberFilterElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExUberFilter
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		int32 NumInside = 0;
		int32 NumOutside = 0;

		PCGExPointFilter::TManager* FilterManager = nullptr;
		FPCGExUberFilterContext* LocalTypedContext = nullptr;

		PCGExMT::FTaskGroup* TestTaskGroup = nullptr;

		PCGEx::TFAttributeWriter<bool>* Results = nullptr;

	public:
		PCGExData::FPointIO* Inside = nullptr;
		PCGExData::FPointIO* Outside = nullptr;

		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
		virtual void Output() override;
	};
}
