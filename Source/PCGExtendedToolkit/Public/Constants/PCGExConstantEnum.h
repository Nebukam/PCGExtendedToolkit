// Copyright 2025 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGExCompare.h"
#include "PCGSettings.h"
#include "PCGExMacros.h"
#include "PCGExGlobalSettings.h"
#include "PCGExContext.h"
#include "PCGParamData.h"
#include "Elements/ControlFlow/PCGControlFlow.h"
#include "PCGExConstantEnum.generated.h"

UENUM(BlueprintType)
enum class EPCGExEnumConstantOutputType : uint8
{
	EECOT_Attribute = 0 UMETA(DisplayName="Attribute Set"),
	EECOT_String    = 1 UMETA(Hidden), // Unsure if this is needed since there's the option to output name and description
	EECOT_Tag       = 2 UMETA(Hidden)  // Hidden for now since this might actually be better as a separate node (Tag With Enum or similar)
};

// TODO (perhaps) - 'Selection' and 'Selection to Multiple Pins'
UENUM(BlueprintType)
enum class EPCGExEnumOutputMode : uint8
{
	EEOM_Single                  = 0 UMETA(DisplayName="Single", Tooltip="Output a single enum value"),
	EEOM_All                     = 1 UMETA(DisplayName="All", ToolTip="Output a dataset containing all the enum names and values"),
	EEOM_AllToMultiplePins       = 2 UMETA(DisplayName="All to Separate Outputs", Tooltip="Output all values in the enum to different pins"),
	EEOM_Selection               = 3 UMETA(DisplayName="Selection", Tooltip="Select values to output as one dataset"),
	EEOM_SelectionToMultiplePins = 4 UMETA(DisplayName="Selection to Separate Outputs", Tooltip="Select values to output to multiple pins")
};

namespace PCGExConstantEnumConstants
{
	using FMapping = TTuple<FName, FName, int64, int32>;
	
	static const FName SingleOutputPinName = "Out";
	static const FName BitflagOutputPinName = "Flags";

	static const FName KeyOutputAttribute = "Key";
	static const FName ValueOutputAttribute = "Value";
	static const FName DescriptionAttribute = "Description";
}

UCLASS(BlueprintType, ClassGroup=(Procedural), meta=(AutoExpandCategories="Settings|Output Attributes"))
class UPCGExConstantEnumSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	PCGEX_DUMMY_SETTINGS_MEMBERS

	// Begin unrolling of Tim's lovely macro
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(EnumConstant, "Enum Constant", "Break an enum into handy constant values.", FName(GetDisplayName())); // Tim says nope! :D
	FString GetDisplayName() const;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; };
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorConstant; };
	// End unrolling of Tim's lovely macro
#endif

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void FillEnabledExportValues();
	
#if PCGEX_ENGINE_VERSION >= 505
	// This does not exist in 5.4
	virtual void OnOverrideSettingsDuplicatedInternal(bool bSkippedPostLoad) override;
#endif

	virtual bool HasDynamicPins() const override { return true; };

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings")
	EPCGExEnumOutputMode OutputMode = EPCGExEnumOutputMode::EEOM_All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ShowOnlyInnerProperties), Category="Settings")
	FEnumSelector SelectedEnum;

	UPROPERTY(
		BlueprintReadWrite, EditAnywhere, Category="Settings", EditFixedSize,
		meta=(
			ReadOnlyKeys,
			EditCondition="OutputMode==EPCGExEnumOutputMode::EEOM_Selection||OutputMode==EPCGExEnumOutputMode::EEOM_SelectionToMultiplePins",
			EditConditionHides
		)
	)
	TMap<FName, bool> EnabledExportValues;


	// Hidden for now
	UPROPERTY(/*BlueprintReadWrite, EditAnywhere, Category=Settings*/)
	EPCGExEnumConstantOutputType OutputType = EPCGExEnumConstantOutputType::EECOT_Attribute;

	// Whether to output the enum value keys, which are the short names used in C++
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Keys")
	bool OutputEnumKeys = false;

	// By default, most (but not all) enum value keys are returned as 'SomeEnum::SomeKey'. If this is true, the key will be output as just 'SomeKey' instead, without the 'SomeEnum::' part, if that is present.  
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Keys", meta=(EditCondition="OutputEnumKeys"))
	bool StripEnumNamespaceFromKey = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Keys", meta=(EditCondition="OutputEnumKeys"))
	FName KeyAttribute = "Key";

	// Whether to output the enum value descriptions, which are the human-readable names for values shown by the UI
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Descriptions")
	bool OutputEnumDescriptions = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Descriptions", meta=(EditCondition="OutputEnumDescriptions"))
	FName DescriptionAttribute = "Description";

	// Whether to output the numeric enum values. Note: will be output as int64 to match behaviour in native PCG
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Values")
	bool OutputEnumValues = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Attributes|Values", meta=(EditCondition="OutputEnumValues"))
	FName ValueOutputAttribute = "Value";

	TArray<PCGExConstantEnumConstants::FMapping> GetEnumValueMap() const;
	UFUNCTION(BlueprintCallable, Category="Config")
	FName GetEnumName() const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Bitflags")
	bool bOutputFlags = false;

	/** Whether to output the enum as a bitmask, and which name should the attribute have in the output attribute set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Bitflags", meta=(EditCondition="bOutputFlags"))
	FName FlagsName = FName("Flags");

	/** Bit to start writing the enum bits to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings|Output Bitflags", meta=(EditCondition="bOutputFlags", ClampMin=0, ClampMax=63))
	uint8 FlagBitOffset = 0;

	// Imitating behaviour in the native PCGSwitch.h
	UPROPERTY()
	TArray<FName> CachedPinLabels;
	void CachePinLabels();

protected:
#if WITH_EDITOR
	virtual EPCGChangeType GetChangeTypeForProperty(const FName& PropName) const override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; };
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
};

class FPCGExConstantEnumElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;

	// Stage to separate pins for each value
	static void StageEnumValuesSeparatePins(
		FPCGExContext* InContext,
		const UPCGExConstantEnumSettings* Settings,
		const TArray<PCGExConstantEnumConstants::FMapping>& ValueData,
		FPCGExBitmask& OutBitflags);

	// Stage all items to a single pin
	static void StageEnumValuesSinglePin(
		FPCGExContext* InContext,
		const UPCGExConstantEnumSettings* Settings,
		const TArray<PCGExConstantEnumConstants::FMapping>& ValueData,
		FPCGExBitmask& OutBitflags);
	
	// Stage bitflags
	static void StageBitFlags(
		FPCGExContext* InContext,
		const UPCGExConstantEnumSettings* Settings,
		FPCGExBitmask& OutBitflags);

	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }
};
