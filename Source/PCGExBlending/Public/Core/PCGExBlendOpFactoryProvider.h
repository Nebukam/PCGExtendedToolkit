// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBlendOpFactory.h"
#include "UObject/Object.h"
#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"

#include "Factories/PCGExFactoryProvider.h"
#include "Metadata/PCGDefaultValueInterface.h"
#include "Utils/PCGExCurveLookup.h"
#include "Utils/PCGExDefaultValueContainer.h"

#include "PCGExBlendOpFactoryProvider.generated.h"

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

namespace PCGExBlending
{
	class FProxyDataBlender;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Blending", meta=(PCGExNodeLibraryDoc="metadata/uber-blend/blend-op"))
class PCGEXBLENDING_API UPCGExBlendOpFactoryProviderSettings : public UPCGExFactoryProviderSettings, public IPCGSettingsDefaultValueProvider
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
#if WITH_EDITOR
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin IPCGSettingsDefaultValueProvider interface
	virtual bool DefaultValuesAreEnabled() const override { return true; }
	virtual bool IsPinDefaultValueEnabled(FName PinLabel) const override;
	virtual bool IsPinDefaultValueActivated(FName PinLabel) const override;
	virtual EPCGMetadataTypes GetPinDefaultValueType(FName PinLabel) const override;
	virtual bool IsPinDefaultValueMetadataTypeValid(FName PinLabel, EPCGMetadataTypes DataType) const override;
#if WITH_EDITOR
	virtual void SetPinDefaultValue(FName PinLabel, const FString& DefaultValue, bool bCreateIfNeeded = false) override;
	virtual void ConvertPinDefaultValueMetadataType(FName PinLabel, EPCGMetadataTypes DataType) override;
	virtual void SetPinDefaultValueIsActivated(FName PinLabel, bool bIsActivated, bool bDirtySettings = true) override;
	virtual void ResetDefaultValues() override;
	virtual FString GetPinInitialDefaultValueString(FName PinLabel) const override;
	virtual FString GetPinDefaultValueAsString(FName PinLabel) const override;
	virtual void ResetDefaultValue(FName PinLabel) override;
#endif // WITH_EDITOR

protected:
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual EPCGMetadataTypes GetPinInitialDefaultValueType(FName PinLabel) const override;
	//~End IPCGSettingsDefaultValueProvider interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(BlendOp, "BlendOp", "Creates a single Blend Operation node, to be used with the Attribute Blender.", PCGEX_FACTORY_NAME_PRIORITY)
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(BlendOp); }
	//PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourceFiltersLabel, "Filters", PCGExFactories::PointFilters, true)

	virtual bool CanUserEditTitle() const override { return false; }
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif


	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoBlendOp)

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	virtual FName GetMainOutputPin() const override { return PCGExBlending::Labels::OutputBlendingLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

	/** Filter Priority.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1))
	int32 Priority = 0;

	/** Config. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeBlendConfig Config;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

private:
	/** Stores the default values for the pins to be used as inline constants. */
	UPROPERTY()
	FPCGExDefaultValueContainer DefaultValues;
};
