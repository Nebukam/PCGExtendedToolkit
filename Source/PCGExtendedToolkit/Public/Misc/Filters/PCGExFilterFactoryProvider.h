// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExFilterFactoryProvider.generated.h"

///

#define PCGEX_CREATE_FILTER_FACTORY(_FILTERID)\
UPCGExParamFactoryBase* UPCGEx##_FILTERID##FilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const{\
	UPCGEx##_FILTERID##FilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGEx##_FILTERID##FilterFactory>();\
	Super::CreateFactory(InContext, InFactory); NewFactory->Config = Config; if(!NewFactory->Init(InContext)){ InContext->ManagedObjects->Destroy(NewFactory); };	return NewFactory; }

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFilterProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		DotFilterFactory, "Filter : Abstract", "Creates an abstract filter definition.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorFilter; }
#endif
	//~End UPCGSettings

public:
	virtual FName GetMainOutputLabel() const override { return PCGExPointFilter::OutputFilterLabel; }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Filter Priority.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority;
};
