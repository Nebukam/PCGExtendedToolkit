﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExGlobalSettings.h"
#include "Data/PCGExPointFilter.h"

#include "PCGExFilterFactoryProvider.generated.h"

///

#define PCGEX_CREATE_FILTER_FACTORY(_FILTERID)\
UPCGExFactoryData* UPCGEx##_FILTERID##FilterProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const{\
	UPCGEx##_FILTERID##FilterFactory* NewFactory = InContext->ManagedObjects->New<UPCGEx##_FILTERID##FilterFactory>();\
	NewFactory->MissingDataHandling = MissingDataHandling;\
	NewFactory->Config = Config; Super::CreateFactory(InContext, NewFactory);\
	if(!NewFactory->Init(InContext)){ InContext->ManagedObjects->Destroy(NewFactory); return nullptr; }\
	return NewFactory; }

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class PCGEXTENDEDTOOLKIT_API UPCGExFilterProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterPoint)

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		AbstractFilterFactory, "Filter : Abstract", "Creates an abstract filter definition.",
		PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorFilter); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputFilterLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
	virtual bool ShouldCancel(FPCGExFactoryProviderContext* InContext, PCGExFactories::EPreparationResult InResult) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Filter Priority.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority;

	/** How to handle missing data. This only applies to filters that rely on data to output meaningful results. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(EditCondition="ShowMissingDataHandling()", PCG_NotOverridable))
	EPCGExFilterNoDataFallback MissingDataHandling = EPCGExFilterNoDataFallback::Fail;

protected:
#if WITH_EDITOR
	virtual bool ShowMissingDataHandling_Internal() const { return false; }

	UFUNCTION()
	bool ShowMissingDataHandling() const { return ShowMissingDataHandling_Internal(); }
#endif
};
