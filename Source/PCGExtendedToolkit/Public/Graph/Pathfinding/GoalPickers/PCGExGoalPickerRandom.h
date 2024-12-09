// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExGoalPickerRandom.generated.h"

struct FPCGExInputConfig;
struct FPCGPoint;
class UPCGPointData;

UENUM()
enum class EPCGExGoalPickRandomAmount : uint8
{
	Single = 0 UMETA(DisplayName = "Single", Tooltip="A single random goal is picked"),
	Fixed  = 1 UMETA(DisplayName = "Multiple Fixed", Tooltip="A fixed number of random goals is picked"),
	Random = 2 UMETA(DisplayName = "Multiple Random", Tooltip="A random number of random goals is picked."),
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Random")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGoalPickerRandom : public UPCGExGoalPicker
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	int32 LocalSeed = 0;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGoalPickRandomAmount GoalCount = EPCGExGoalPickRandomAmount::Single;

	/** Fetch the smoothing from a local attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType NumGoalsType = EPCGExInputValueType::Constant;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single && NumGoalsType==EPCGExInputValueType::Constant", ClampMin=1))
	int32 NumGoals = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single && NumGoalsType==EPCGExInputValueType::Attribute"))
	FPCGAttributePropertyInputSelector NumGoalAttribute;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade) override;

	virtual int32 GetGoalIndex(const PCGExData::FPointRef& Seed) const override;
	virtual void GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;

	virtual void Cleanup() override;

protected:
	TSharedPtr<PCGExData::TBuffer<int32>> NumGoalsGetter;
};
