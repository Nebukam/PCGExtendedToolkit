// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExPicker.h"
#include "PCGExPickerFactoryProvider.h"
#include "PCGExPickerOperation.h"

#include "PCGExPickerConstant.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPickerConstantConfig : public FPCGExPickerConfigBase
{
	GENERATED_BODY()

	FPCGExPickerConstantConfig() :
		FPCGExPickerConfigBase()
	{
	}

	/**  Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="!bTreatAsNormalized", EditConditionHides))
	int32 DiscreteIndex = 0;

	/**  Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTreatAsNormalized", EditConditionHides))
	double RelativeIndex = 0;
};

/**
 * 
 */
UCLASS(MinimalAPI)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerConstant : public UPCGExPickerOperation
{
	GENERATED_BODY()

public:
	FPCGExPickerConstantConfig Config;
	virtual bool Init(FPCGExContext* InContext, const UPCGExPickerFactoryData* InFactory) override;

	virtual void AddPicks(const TSharedRef<PCGExData::FFacade>& InDataFacade, TSet<int32>& OutPicks) const override;
};


UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerConstantFactory : public UPCGExPickerFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExPickerConstantConfig Config;

	virtual UPCGExPickerOperation* CreateOperation(FPCGExContext* InContext) const override;

protected:
	virtual bool InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Pickers|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerConstantSettings : public UPCGExPickerFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PickerConstant, "Picker : Constant", "A Picker that has a single value.",
		FName(GetDisplayName()))

#endif
	//~End UPCGSettings

	/** Picker properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPickerConstantConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};
