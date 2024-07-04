// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExUberFilter.generated.h"

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
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetPointFilterLabel() const override;
	virtual bool RequiresPointFilters() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Swap Inside & Outside data */
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

		PCGExData::FPointIOCollection* InCollection = nullptr;
		PCGExData::FPointIOCollection* OutCollection = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};
}
