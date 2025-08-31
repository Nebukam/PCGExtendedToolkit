﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFactoryProvider.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTensor.h"

#include "PCGExTensorFactoryProvider.generated.h"

#define PCGEX_TENSOR_BOILERPLATE(_TENSOR, _NEW_FACTORY, _NEW_OPERATION) \
TSharedPtr<PCGExTensorOperation> UPCGExTensor##_TENSOR##Factory::CreateOperation(FPCGExContext* InContext) const{ \
	PCGEX_FACTORY_NEW_OPERATION(Tensor##_TENSOR)\
	NewOperation->Factory = this; \
	NewOperation->Config = Config; \
	_NEW_OPERATION \
	NewOperation->BaseConfig = NewOperation->Config; \
	if(!NewOperation->Init(InContext, this)){ return nullptr; } \
	return NewOperation; } \
UPCGExFactoryData* UPCGExCreateTensor##_TENSOR##Settings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ \
	UPCGExTensor##_TENSOR##Factory* NewFactory = InContext->ManagedObjects->New<UPCGExTensor##_TENSOR##Factory>(); \
	NewFactory->Config = Config; \
	Super::CreateFactory(InContext, NewFactory); /* Super factory to grab custom override settings before body */ \
	_NEW_FACTORY \
	NewFactory->Config.Init(); \
	NewFactory->BaseConfig = NewFactory->Config; \
	return NewFactory; }

class PCGExTensorOperation;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExTensorFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

	friend class UPCGExTensorFactoryProviderSettings;

	// TODO : To favor re-usability Tensor factories hold more complex logic than regular factories
	// They are also samplers
	// We leverage internal point data and pack all needed attributes & computed points inside

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Tensor; }
	virtual TSharedPtr<PCGExTensorOperation> CreateOperation(FPCGExContext* InContext) const;

	FPCGExTensorConfigBase BaseConfig;
	virtual PCGExFactories::EPreparationResult Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;

protected:
	virtual PCGExFactories::EPreparationResult InitInternalData(FPCGExContext* InContext);
	virtual void InheritFromOtherTensor(const UPCGExTensorFactoryData* InOtherTensor);
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExTensorFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Tensor, "Tensor Definition", "Creates a single tensor field definition.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTensor; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	/** Tensor Priority, only accounted for by if sampler is in any Ordered- mode.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1), AdvancedDisplay)
	int32 Priority = 0;

	virtual FName GetMainOutputPin() const override { return PCGExTensor::OutputTensorLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};


UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExTensorPointFactoryData : public UPCGExTensorFactoryData
{
	GENERATED_BODY()

public:
	TSharedPtr<PCGExData::FFacade> InputDataFacade;
	TSharedPtr<PCGExTensor::FEffectorsArray> EffectorsArray;

protected:
	virtual bool WantsPreparation(FPCGExContext* InContext) override { return true; }
	virtual PCGExFactories::EPreparationResult InitInternalData(FPCGExContext* InContext) override;

	virtual TSharedPtr<PCGExTensor::FEffectorsArray> GetEffectorsArray() const;

	virtual bool InitInternalFacade(FPCGExContext* InContext);
	virtual void PrepareSinglePoint(int32 Index) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExTensorPointFactoryProviderSettings : public UPCGExTensorFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
};
