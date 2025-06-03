// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExPicker.h"
#include "PCGExPickerFactoryProvider.h"

#include "PCGExPickerConstantRange.generated.h"

USTRUCT(BlueprintType)
struct FPCGExPickerConstantRangeConfig : public FPCGExPickerConfigBase
{
	GENERATED_BODY()

	FPCGExPickerConstantRangeConfig() :
		FPCGExPickerConfigBase()
	{
	}

	/**  If enabled, ensure that whatever values are used for start and end, they are ordered to form a valid range. i.e, [5,1] will be processed as [1,5] */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bAutoSortRange = true;

	/**  Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bTreatAsNormalized", EditConditionHides, DisplayAfter="bTreatAsNormalized"))
	int32 DiscreteStartIndex = 0;

	/**  Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bTreatAsNormalized", EditConditionHides, DisplayAfter="bTreatAsNormalized"))
	int32 DiscreteEndIndex = 0;

	/**  Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTreatAsNormalized", EditConditionHides, DisplayAfter="bTreatAsNormalized"))
	double RelativeStartIndex = 0;

	/**  Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTreatAsNormalized", EditConditionHides, DisplayAfter="bTreatAsNormalized"))
	double RelativeEndIndex = 0;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExPickerConstantRangeFactory : public UPCGExPickerFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExPickerConstantRangeConfig Config;

	virtual void AddPicks(int32 InNum, TSet<int32>& OutPicks) const override;

protected:
	virtual bool InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Pickers|Params")
class UPCGExPickerConstantRangeSettings : public UPCGExPickerFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PickerConstantRange, "Picker : Range", "A Picker that selects a range of values.",
		FName(GetDisplayName()))

#endif
	//~End UPCGSettings

	/** Picker properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPickerConstantRangeConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

};
