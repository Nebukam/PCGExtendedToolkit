// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGoalPickerRandom.generated.h"

struct FPCGExInputDescriptor;
struct FPCGPoint;
class UPCGPointData;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Goal Pick Random - Amount"))
enum class EPCGExGoalPickRandomAmount : uint8
{
	Single UMETA(DisplayName = "Single", Tooltip="A single random goal is picked"),
	Fixed UMETA(DisplayName = "Multiple Fixed", Tooltip="A fixed number of random goals is picked"),
	Random UMETA(DisplayName = "Multiple Random", Tooltip="A random number of random goals is picked."),
};

/**
 * 
 */
UCLASS(DisplayName = "Random")
class PCGEXTENDEDTOOLKIT_API UPCGExGoalPickerRandom : public UPCGExGoalPicker
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExGoalPickRandomAmount GoalCount = EPCGExGoalPickRandomAmount::Single;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single", ClampMin=1))
	int32 NumGoals = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single"))
	bool bUseLocalNumGoals = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single && bUseLocalNumGoals"))
	FPCGAttributePropertyInputSelector LocalNumGoalAttribute;

	virtual void PrepareForData(const PCGExData::FPointIO& InSeeds, const PCGExData::FPointIO& InGoals) override;

	virtual int32 GetGoalIndex(const PCGEx::FPointRef& Seed) const override;
	virtual void GetGoalIndices(const PCGEx::FPointRef& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;

	virtual void Cleanup() override;

protected:
	PCGEx::FLocalIntegerGetter* NumGoalsGetter = nullptr;
	virtual void ApplyOverrides() override;
};
