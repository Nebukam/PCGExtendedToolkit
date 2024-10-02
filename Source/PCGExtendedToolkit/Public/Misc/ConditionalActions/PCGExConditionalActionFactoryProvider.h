// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"
#include "Data/PCGExPointFilter.h"


#include "PCGExConditionalActionFactoryProvider.generated.h"

#define PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(_NAME, _BODY) \
	UPCGExParamFactoryBase* UPCGEx##_NAME##ProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const{ \
	UPCGEx##_NAME##Factory* NewFactory = NewObject<UPCGEx##_NAME##Factory>(); _BODY if(!Super::CreateFactory(InContext, NewFactory)){ PCGEX_DELETE_UOBJECT(NewFactory) } return NewFactory; }

#define PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(_NAME, _BODY) \
	UPCGExConditionalActionOperation* UPCGEx##_NAME##Factory::CreateOperation(FPCGExContext* InContext) const{ \
	UPCGEx##_NAME##Operation* NewOperation = InContext->PCGExNewObject<UPCGEx##_NAME##Operation>(this->GetOuter()); \
	NewOperation->TypedFactory = const_cast<UPCGEx##_NAME##Factory*>(this); \
	NewOperation->Factory = NewOperation->TypedFactory; \
	_BODY \
	return NewOperation;}

namespace PCGExConditionalActions
{
	const FName SourceConditionsFilterLabel = TEXT("Conditions");
	const FName SourceConditionalActionsLabel = TEXT("Actions");
	const FName SourceDefaultsLabel = TEXT("Default values");
	const FName OutputConditionalActionLabel = TEXT("Action");
}


/**
 * 
 */
UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPCGExConditionalActionFactoryBase* Factory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade);
	virtual void ProcessPoint(int32 Index, const FPCGPoint& Point);

	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point);
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point);

	virtual void Cleanup() override;

protected:
	TUniquePtr<PCGExPointFilter::TManager> FilterManager;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	TSharedPtr<PCGEx::FAttributesInfos> CheckSuccessInfos;
	TSharedPtr<PCGEx::FAttributesInfos> CheckFailInfos;

	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> FilterFactories;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::ConditionalActions; }
	virtual UPCGExConditionalActionOperation* CreateOperation(FPCGExContext* InContext) const;

	virtual bool Boot(FPCGContext* InContext);
	virtual bool AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage) const;

	virtual void BeginDestroy() override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|ConditionalAction")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExConditionalActionProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConditionalActionAbstract, "Action : Abstract", "Abstract Match To Module.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override { return PCGExConditionalActions::OutputConditionalActionLabel; }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Priority for transmutation order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;
};
