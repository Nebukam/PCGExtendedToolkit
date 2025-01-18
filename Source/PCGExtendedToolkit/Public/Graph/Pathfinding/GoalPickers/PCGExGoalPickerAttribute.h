// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.h"
#include "Data/PCGExAttributeHelpers.h"


#include "PCGExGoalPickerAttribute.generated.h"

struct FPCGPoint;
class UPCGPointData;

UENUM()
enum class EPCGExGoalPickAttributeAmount : uint8
{
	Single = 0 UMETA(DisplayName = "Single Attribute", Tooltip="Single attribute"),
	List   = 1 UMETA(DisplayName = "Multiple Attributes", Tooltip="Multiple attributes"),
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Index Attribute")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGoalPickerAttribute : public UPCGExGoalPicker
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGoalPickAttributeAmount GoalCount = EPCGExGoalPickAttributeAmount::Single;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount==EPCGExGoalPickAttributeAmount::Single", EditConditionHides))
	FPCGAttributePropertyInputSelector SingleSelector;
	TSharedPtr<PCGExData::TBuffer<int32>> SingleGetter;

	/** A list of attribute names separated by a comma, for easy overrides. They will be added to the in-place array of selectors. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="GoalCount==EPCGExGoalPickAttributeAmount::List"))
	FString CommaSeparatedNames = TEXT("");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount==EPCGExGoalPickAttributeAmount::List", EditConditionHides, TitleProperty="{TitlePropertyName}"))
	TArray<FPCGAttributePropertyInputSelector> AttributeSelectors;
	TArray<TSharedPtr<PCGExData::TBuffer<int32>>> AttributeGetters;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade) override;
	virtual int32 GetGoalIndex(const PCGExData::FPointRef& Seed) const override;
	virtual void GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;

	virtual void Cleanup() override
	{
		SingleGetter.Reset();
		AttributeGetters.Empty();
		Super::Cleanup();
	}
};
