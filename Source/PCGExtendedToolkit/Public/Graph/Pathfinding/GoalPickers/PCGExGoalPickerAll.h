// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExGoalPicker.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExGoalPickerAll.generated.h"

struct FPCGExInputConfig;
struct FPCGPoint;
class UPCGPointData;

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "All")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGoalPickerAll : public UPCGExGoalPicker
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InSeedsDataFacade, const TSharedPtr<PCGExData::FFacade>& InGoalsDataFacade) override;

	virtual void GetGoalIndices(const PCGExData::FPointRef& Seed, TArray<int32>& OutIndices) const override;
	virtual bool OutputMultipleGoals() const override;

	virtual void Cleanup() override;

protected:
	int32 GoalsNum = -1;
};
