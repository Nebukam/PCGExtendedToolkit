﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExGoalPickerRandom.generated.h"

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
UCLASS(MinimalAPI, meta=(DisplayName = "Random", PCGExNodeLibraryDoc="pathfinding/pathfinding-edges/goal-picker-random"))
class UPCGExGoalPickerRandom : public UPCGExGoalPicker
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Num Goals (Attr)", EditCondition="GoalCount != EPCGExGoalPickRandomAmount::Single && NumGoalsType != EPCGExInputValueType::Constant"))
	FPCGAttributePropertyInputSelector NumGoalAttribute;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Num Goals", EditCondition="GoalCount != EPCGExGoalPickRandomAmount::Single && NumGoalsType == EPCGExInputValueType::Constant", ClampMin=1))
	int32 NumGoals = 5;

	virtual void CopySettingsFrom(const UPCGExInstancedFactory* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade) override;

	virtual int32 GetGoalIndex(const PCGExData::FConstPoint& Seed) const override;
	virtual void GetGoalIndices(const PCGExData::FConstPoint& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;

	virtual void Cleanup() override;

	PCGEX_SETTING_VALUE_DECL(NumGoals, int32)

protected:
	TSharedPtr<PCGExDetails::TSettingValue<int32>> NumGoalsBuffer;
};
