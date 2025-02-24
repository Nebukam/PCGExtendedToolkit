// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExPicker.h"
#include "PCGExPickerFactoryProvider.h"

#include "PCGExPickerConstantSet.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPickerConstantSetConfig : public FPCGExPickerConfigBase
{
	GENERATED_BODY()

	FPCGExPickerConstantSetConfig() :
		FPCGExPickerConfigBase()
	{
	}

	/** List of attributes to read indices from. Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TArray<FPCGAttributePropertyInputSelector> Attributes;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerConstantSetFactory : public UPCGExPickerFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExPickerConstantSetConfig Config;

	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual void AddPicks(int32 InNum, TSet<int32>& OutPicks) const override;

protected:
	virtual bool InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Pickers|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerConstantSetSettings : public UPCGExPickerFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PickerConstantSet, "Picker : Constant Set", "A Picker that accept lists of values, read from an attribute.")

#endif
	//~End UPCGSettings

	/** Picker properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPickerConstantSetConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

protected:
	virtual bool IsCacheable() const override { return true; }
};
