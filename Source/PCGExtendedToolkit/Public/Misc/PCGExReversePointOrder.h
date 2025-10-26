﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSorting.h"
#include "Geometry/PCGExGeo.h"


#include "PCGExReversePointOrder.generated.h"

namespace PCGExData
{
	class IBuffer;
}

UENUM()
enum class EPCGExPointReverseMethod : uint8
{
	None         = 0 UMETA(DisplayName = "None", ToolTip="..."),
	SortingRules = 1 UMETA(DisplayName = "Sorting Rules", ToolTip="..."),
	Winding      = 2 UMETA(DisplayName = "Winding", ToolTip="..."),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSwapAttributePairDetails
{
	GENERATED_BODY()

	FPCGExSwapAttributePairDetails()
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName FirstAttributeName = NAME_None;
	PCGEx::FAttributeIdentity* FirstIdentity = nullptr;
	TSharedPtr<PCGExData::IBuffer> FirstWriter;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName SecondAttributeName = NAME_None;
	PCGEx::FAttributeIdentity* SecondIdentity = nullptr;
	TSharedPtr<PCGExData::IBuffer> SecondWriter;

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

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="misc/reverse-point-order"))
class UPCGExReversePointOrderSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ReversePointOrder, "Reverse Point Order", "Simply reverse the order of points. Very useful with paths.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorMiscWrite); }
#endif

protected:
	virtual PCGExData::EIOInit GetIOPreInitForMainPoints() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPointReverseMethod Method = EPCGExPointReverseMethod::None;

	/** Sort direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Method == EPCGExPointReverseMethod::SortingRules", EditConditionHides))
	EPCGExSortDirection SortDirection = EPCGExSortDirection::Ascending;

	/** Winding */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Method == EPCGExPointReverseMethod::Winding", EditConditionHides))
	EPCGExWinding Winding = EPCGExWinding::CounterClockwise;

	/** Projection settings. Winding is computed on a 2D plane. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Method == EPCGExPointReverseMethod::Winding", EditConditionHides))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails();

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExSwapAttributePairDetails> SwapAttributesValues;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfReversed = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfReversed"))
	FString IsReversedTag = TEXT("Reversed");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIfNotReversed = false;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIfNotReversed"))
	FString IsNotReversedTag = TEXT("NotReversed");
};

struct FPCGExReversePointOrderContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExReversePointOrderElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExReversePointOrderElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ReversePointOrder)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExReversePointOrder
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExReversePointOrderContext, UPCGExReversePointOrderSettings>
	{
		TArray<FPCGExSwapAttributePairDetails> SwapPairs;
		TSharedPtr<PCGExSorting::FPointSorter> Sorter;

		FPCGExGeo2DProjectionDetails ProjectionDetails;

		bool bReversed = true;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}
