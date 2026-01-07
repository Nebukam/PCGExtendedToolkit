// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPickerFactoryProvider.h"

#include "PCGExPickerAttributeSet.generated.h"

USTRUCT(BlueprintType)
struct FPCGExPickerAttributeSetConfig : public FPCGExPickerConfigBase
{
	GENERATED_BODY()

	FPCGExPickerAttributeSetConfig()
		: FPCGExPickerConfigBase()
	{
	}

	/** List of attributes to read individual indices from. Use negative values to select from the end. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TArray<FPCGAttributePropertyInputSelector> Attributes;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data", meta=(PCGExNodeLibraryDoc="filters/cherry-pick-points/picker-constant-set"))
class UPCGExPickerAttributeSetFactory : public UPCGExPickerFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExPickerAttributeSetConfig Config;

	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual void AddPicks(int32 InNum, TSet<int32>& OutPicks) const override;

protected:
	virtual PCGExFactories::EPreparationResult InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Pickers|Params")
class UPCGExPickerAttributeSetSettings : public UPCGExPickerFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PickerConstantSet, "Picker : Indices from Set", "A Picker that accept lists of values, read from one of more attribute. Note that if no attribute is set in the details, it will use the first available one.")

#endif
	//~End UPCGSettings

	/** Picker properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPickerAttributeSetConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};
