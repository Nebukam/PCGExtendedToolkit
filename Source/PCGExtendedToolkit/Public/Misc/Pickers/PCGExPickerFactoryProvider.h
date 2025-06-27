// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExPicker.h"
#include "Sampling/PCGExSampleNearestSpline.h"

#include "PCGExPickerFactoryProvider.generated.h"

class UPCGExPickerOperation;

#define PCGEX_PICKER_BOILERPLATE(_PICKER, _NEW_FACTORY, _NEW_OPERATION) \
UPCGExFactoryData* UPCGExPicker##_PICKER##Settings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ \
UPCGExPicker##_PICKER##Factory* NewFactory = InContext->ManagedObjects->New<UPCGExPicker##_PICKER##Factory>(); \
NewFactory->Config = Config; \
NewFactory->Config.Sanitize(); \
Super::CreateFactory(InContext, NewFactory); /* Super factory to grab custom override settings before body */ \
_NEW_FACTORY \
return NewFactory; }

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExPickerFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

	friend class UPCGExPickerFactoryProviderSettings;

	// TODO : To favor re-usability Picker factories hold more complex logic than regular factories
	// They are also samplers
	// We leverage internal point data and pack all needed attributes & computed points inside

public:
	UPROPERTY()
	TArray<int32> DiscretePicks;

	UPROPERTY()
	TArray<double> RelativePicks;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::IndexPicker; }

	virtual void AddPicks(int32 InNum, TSet<int32>& OutPicks) const;

	FPCGExPickerConfigBase BaseConfig;
	virtual bool Prepare(FPCGExContext* InContext) override;

protected:
	virtual bool RequiresInputs() const;
	virtual bool WantsPreparation(FPCGExContext* InContext) override { return false; }
	virtual bool InitInternalData(FPCGExContext* InContext);
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExPickerFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Picker, "Picker Definition", "Creates a single Picker definition.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputPin() const override { return PCGExPicker::OutputPickerLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};
