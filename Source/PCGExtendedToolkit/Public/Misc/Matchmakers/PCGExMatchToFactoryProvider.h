// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExOperation.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExMatchToFactoryProvider.generated.h"

#define PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(_NAME, _BODY) \
	UPCGExParamFactoryBase* UPCGEx##_NAME##ProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const{ \
	UPCGEx##_NAME##Factory* NewFactory = NewObject<UPCGEx##_NAME##Factory>(); _BODY Super::CreateFactory(InContext, NewFactory); return NewFactory; }

#define PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(_NAME, _BODY) \
UPCGExMatchToOperation* UPCGEx##_NAME##Factory::CreateOperation() const{ \
	UPCGEx##_NAME##Operation* NewOperation = NewObject<UPCGEx##_NAME##Operation>(); \
	NewOperation->Factory = const_cast<UPCGEx##_NAME##Factory*>(this); _BODY \
	return NewOperation;}

class UPCGExFilterFactoryBase;

namespace PCGExMatchmaking
{
	const FName SourceMatchFilterLabel = TEXT("MatchFilters");
	const FName SourceMatchmakersLabel = TEXT("Matchmakers");
	const FName SourceDefaultsLabel = TEXT("Default values");
	const FName OutputMatchmakerLabel = TEXT("Matchmaker");
}

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExMatchToOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	UPCGExMatchToFactoryBase* Factory = nullptr;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;

	virtual bool PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade);
	virtual void ProcessPoint(int32 Index, const FPCGPoint& Point);

	virtual void OnMatchSuccess(int32 Index, const FPCGPoint& Point);
	virtual void OnMatchFail(int32 Index, const FPCGPoint& Point);

	virtual void Cleanup() override;

protected:
	PCGExPointFilter::TManager* FilterManager = nullptr;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchToFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExAttributeGatherDetails InputAttributesFilter;

	PCGEx::FAttributesInfos* CheckSuccessInfos = nullptr;
	PCGEx::FAttributesInfos* CheckFailInfos = nullptr;

	TArray<UPCGExFilterFactoryBase*> FilterFactories;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::Matchmaking; }
	virtual UPCGExMatchToOperation* CreateOperation() const;

	virtual bool Boot(FPCGContext* InContext);
	virtual bool AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage);

	virtual void BeginDestroy() override;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|MatchTo")
class PCGEXTENDEDTOOLKIT_API UPCGExMatchToProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MatchToAbstract, "Match To : Abstract", "Abstract Match To Module.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMisc; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override { return PCGExMatchmaking::OutputMatchmakerLabel; }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Priority for transmutation order. Higher values are processed last. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExAttributeGatherDetails InputAttributesFilter;
};
