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
UPCGExPickerOperation* UPCGExPicker##_PICKER##Factory::CreateOperation(FPCGExContext* InContext) const{ \
UPCGExPicker##_PICKER* NewOperation = InContext->ManagedObjects->New<UPCGExPicker##_PICKER>(); \
NewOperation->Factory = this; \
NewOperation->Config = Config; \
_NEW_OPERATION \
NewOperation->BaseConfig = NewOperation->Config; \
if(!NewOperation->Init(InContext, this)){ return nullptr; } \
return NewOperation; } \
UPCGExFactoryData* UPCGExPicker##_PICKER##Settings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ \
UPCGExPicker##_PICKER##Factory* NewFactory = InContext->ManagedObjects->New<UPCGExPicker##_PICKER##Factory>(); \
NewFactory->Config = Config; \
Super::CreateFactory(InContext, NewFactory); /* Super factory to grab custom override settings before body */ \
_NEW_FACTORY \
return NewFactory; }

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

	friend class UPCGExPickerFactoryProviderSettings;

	// TODO : To favor re-usability Picker factories hold more complex logic than regular factories
	// They are also samplers
	// We leverage internal point data and pack all needed attributes & computed points inside

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::IndexPicker; }
	virtual UPCGExPickerOperation* CreateOperation(FPCGExContext* InContext) const;

	FPCGExPickerConfigBase BaseConfig;
	virtual bool Prepare(FPCGExContext* InContext) override;

protected:
	virtual bool InitInternalData(FPCGExContext* InContext);
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerFactoryProviderSettings : public UPCGExFactoryProviderSettings
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


UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerPointFactoryData : public UPCGExPickerFactoryData
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> DiscretePicks;

	UPROPERTY()
	TArray<int32> RelativePicks;

protected:
	virtual bool GetRequiresPreparation(FPCGExContext* InContext) override { return false; }
	virtual bool InitInternalData(FPCGExContext* InContext) override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPickerPointFactoryProviderSettings : public UPCGExPickerFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	virtual bool RequiresInputs() const;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
};
