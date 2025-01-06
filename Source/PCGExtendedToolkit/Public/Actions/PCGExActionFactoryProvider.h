// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"
#include "Data/PCGExPointFilter.h"


#include "PCGExActionFactoryProvider.generated.h"

#define PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(_NAME, _BODY) \
	UPCGExFactoryData* UPCGEx##_NAME##ProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{ \
	UPCGEx##_NAME##Factory* NewFactory = NewObject<UPCGEx##_NAME##Factory>(); _BODY if(!Super::CreateFactory(InContext, NewFactory)){ InContext->ManagedObjects->Destroy(NewFactory); } return NewFactory; }

#define PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(_NAME, _BODY) \
	UPCGExActionOperation* UPCGEx##_NAME##Factory::CreateOperation(FPCGExContext* InContext) const{ \
	UPCGEx##_NAME##Operation* NewOperation = InContext->ManagedObjects->New<UPCGEx##_NAME##Operation>(this->GetOuter()); \
	NewOperation->TypedFactory = const_cast<UPCGEx##_NAME##Factory*>(this); \
	NewOperation->Factory = NewOperation->TypedFactory; \
	_BODY \
	return NewOperation;}

class UPCGExActionFactoryData;

namespace PCGExActions
{
	const FName SourceConditionsFilterLabel = TEXT("Conditions");
	const FName SourceActionsLabel = TEXT("Actions");
	const FName SourceDefaultsLabel = TEXT("Default values");
	const FName OutputActionLabel = TEXT("Action");
}


/**
 * 
 */
UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActionOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPCGExActionFactoryData* Factory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade);
	virtual void ProcessPoint(int32 Index, const FPCGPoint& Point);

	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point);
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point);

	virtual void Cleanup() override;

protected:
	TSharedPtr<PCGExPointFilter::FManager> FilterManager;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActionFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	TSharedPtr<PCGEx::FAttributesInfos> CheckSuccessInfos;
	TSharedPtr<PCGEx::FAttributesInfos> CheckFailInfos;

	TArray<TObjectPtr<const UPCGExFilterFactoryData>> FilterFactories;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Action; }
	virtual UPCGExActionOperation* CreateOperation(FPCGExContext* InContext) const;

	virtual bool Boot(FPCGContext* InContext);
	virtual bool AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage) const;

	virtual void BeginDestroy() override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Action")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActionProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ActionAbstract, "Action : Abstract", "Abstract Action Provider.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual bool GetRequiresFilters() const { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputPin() const override { return PCGExActions::OutputActionLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Priority for transmutation order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;
};
