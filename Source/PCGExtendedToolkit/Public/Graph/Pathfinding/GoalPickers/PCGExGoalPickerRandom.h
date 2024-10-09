// Copyright Timothé Lapetite 2024
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

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Goal Pick Random - Amount"))
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GoalPicker)
	EPCGExGoalPickRandomAmount GoalCount = EPCGExGoalPickRandomAmount::Single;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GoalPicker, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single", ClampMin=1))
	int32 NumGoals = 5;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GoalPicker, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single"))
	bool bUseLocalNumGoals = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GoalPicker, meta=(EditCondition="GoalCount!=EPCGExGoalPickRandomAmount::Single && bUseLocalNumGoals"))
	FPCGAttributePropertyInputSelector LocalNumGoalAttribute;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual void PrepareForData(const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade) override;

	virtual int32 GetGoalIndex(const PCGExData::FPointRef& Seed) const override;
	virtual void GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;

	virtual void Cleanup() override;

protected:
	TSharedPtr<PCGExData::TBuffer<int32>> NumGoalsGetter;

	virtual void ApplyOverrides() override
	{
		Super::ApplyOverrides();
		PCGEX_OVERRIDE_OPERATION_PROPERTY(NumGoals, "Goals/NumGoals")
		PCGEX_OVERRIDE_OPERATION_PROPERTY(bUseLocalNumGoals, "Goals/UseLocalNumGoals")
	}
};
