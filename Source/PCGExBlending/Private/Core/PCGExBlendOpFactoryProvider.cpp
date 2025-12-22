// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExBlendOpFactoryProvider.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"

#define LOCTEXT_NAMESPACE "PCGExCreateAttributeBlend"
#define PCGEX_NAMESPACE CreateAttributeBlend

#if WITH_EDITOR
void UPCGExBlendOpFactoryProviderSettings::ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins)
{
	Super::ApplyDeprecationBeforeUpdatePins(InOutNode, InputPins, OutputPins);
	InOutNode->RenameInputPin(FName("Constant A"), PCGExBlending::Labels::SourceConstantA);
	InOutNode->RenameInputPin(FName("Constant B"), PCGExBlending::Labels::SourceConstantB);
}

void UPCGExBlendOpFactoryProviderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Config.bRequiresWeight = Config.BlendMode == EPCGExABBlendingType::Lerp || Config.BlendMode == EPCGExABBlendingType::Weight || Config.BlendMode == EPCGExABBlendingType::WeightedSubtract || Config.BlendMode == EPCGExABBlendingType::WeightedAdd;

	FName Prop = PropertyChangedEvent.GetMemberPropertyName();
	if (Prop == GET_MEMBER_NAME_CHECKED(FPCGExAttributeBlendConfig, OperandASource) && Config.OperandASource == EPCGExOperandSource::Constant)
	{
	}
	else if (Prop == GET_MEMBER_NAME_CHECKED(FPCGExAttributeBlendConfig, OperandBSource) && Config.OperandBSource == EPCGExOperandSource::Constant)
	{
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

#pragma region Inline pins

bool UPCGExBlendOpFactoryProviderSettings::IsPinDefaultValueEnabled(FName PinLabel) const
{
	return PinLabel == PCGExBlending::Labels::SourceConstantA || PinLabel == PCGExBlending::Labels::SourceConstantB;
}

bool UPCGExBlendOpFactoryProviderSettings::IsPinDefaultValueActivated(FName PinLabel) const
{
	if (!IsPinDefaultValueEnabled(PinLabel)) { return false; }
	return DefaultValues.IsPropertyActivated(PinLabel);
}

EPCGMetadataTypes UPCGExBlendOpFactoryProviderSettings::GetPinDefaultValueType(FName PinLabel) const
{
	if (DefaultValues.FindProperty(PinLabel)) { return DefaultValues.GetCurrentPropertyType(PinLabel); }
	return GetPinInitialDefaultValueType(PinLabel);
}

bool UPCGExBlendOpFactoryProviderSettings::IsPinDefaultValueMetadataTypeValid(FName PinLabel, EPCGMetadataTypes DataType) const
{
	// We support any value type so whatever
	return true;
}

#if WITH_EDITOR
void UPCGExBlendOpFactoryProviderSettings::SetPinDefaultValue(FName PinLabel, const FString& DefaultValue, bool bCreateIfNeeded)
{
	Modify();

	if (!DefaultValues.FindProperty(PinLabel) && bCreateIfNeeded)
	{
		const EPCGMetadataTypes Type = GetPinInitialDefaultValueType(PinLabel);
		DefaultValues.CreateNewProperty(PinLabel, Type);
	}

	if (DefaultValues.SetPropertyValueFromString(PinLabel, DefaultValue))
	{
		OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Node | EPCGChangeType::Edge);
	}
}

void UPCGExBlendOpFactoryProviderSettings::ConvertPinDefaultValueMetadataType(FName PinLabel, EPCGMetadataTypes DataType)
{
	if (ensure(IsPinDefaultValueActivated(PinLabel)))
	{
		if (IsPinDefaultValueMetadataTypeValid(PinLabel, DataType))
		{
			Modify();
			DefaultValues.ConvertPropertyType(PinLabel, DataType);
			OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Node | EPCGChangeType::Edge);
		}
	}
}

void UPCGExBlendOpFactoryProviderSettings::SetPinDefaultValueIsActivated(FName PinLabel, bool bIsActivated, bool bDirtySettings)
{
	if (ensure(IsPinDefaultValueEnabled(PinLabel)))
	{
		if (bDirtySettings) { Modify(); }

		const bool bPropertyChanged = DefaultValues.SetPropertyActivated(PinLabel, bIsActivated);
		if (bPropertyChanged && bDirtySettings)
		{
			OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Node | EPCGChangeType::Edge);
		}
	}
}

void UPCGExBlendOpFactoryProviderSettings::ResetDefaultValues()
{
	DefaultValues.Reset();
	OnSettingsChangedDelegate.Broadcast(this, EPCGChangeType::Settings | EPCGChangeType::Edge);
}

FString UPCGExBlendOpFactoryProviderSettings::GetPinInitialDefaultValueString(FName PinLabel) const
{
	return LexToString(1.0f);
}

FString UPCGExBlendOpFactoryProviderSettings::GetPinDefaultValueAsString(FName PinLabel) const
{
	if (ensure(IsPinDefaultValueActivated(PinLabel)))
	{
		if (DefaultValues.FindProperty(PinLabel)) { return DefaultValues.GetPropertyValueAsString(PinLabel); }
		return GetPinInitialDefaultValueString(PinLabel);
	}

	return FString();
}

void UPCGExBlendOpFactoryProviderSettings::ResetDefaultValue(FName PinLabel)
{
	if (DefaultValues.FindProperty(PinLabel))
	{
		Modify();
		const EPCGMetadataTypes CurrentType = DefaultValues.GetCurrentPropertyType(PinLabel);
		DefaultValues.RemoveProperty(PinLabel);
		DefaultValues.CreateNewProperty(PinLabel, CurrentType);
	}
}
#endif

bool UPCGExBlendOpFactoryProviderSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExBlending::Labels::SourceConstantA && Config.OperandASource == EPCGExOperandSource::Constant) { return true; }
	if (InPin->Properties.Label == PCGExBlending::Labels::SourceConstantB && Config.OperandBSource == EPCGExOperandSource::Constant) { return true; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

EPCGMetadataTypes UPCGExBlendOpFactoryProviderSettings::GetPinInitialDefaultValueType(FName PinLabel) const
{
	return EPCGMetadataTypes::Float;
}

#pragma endregion

#if WITH_EDITOR
TArray<FPCGPreConfiguredSettingsInfo> UPCGExBlendOpFactoryProviderSettings::GetPreconfiguredInfo() const
{
	const TSet ValuesToSkip = {EPCGExABBlendingType::None};
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExABBlendingType>(ValuesToSkip, FTEXT("Blend : {0}"));
}
#endif

void UPCGExBlendOpFactoryProviderSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExABBlendingType>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Config.BlendMode = static_cast<EPCGExABBlendingType>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

TArray<FPCGPinProperties> UPCGExBlendOpFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY_SINGLE(PCGExBlending::Labels::SourceConstantA, "Data used to read a constant from. Will read from the first element of the first data.", Advanced)

	if (Config.bUseOperandB)
	{
		PCGEX_PIN_ANY_SINGLE(PCGExBlending::Labels::SourceConstantB, "Data used to read a constant from. Will read from the first element of the first data.", Advanced)
	}

	return PinProperties;
}

UPCGExFactoryData* UPCGExBlendOpFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExBlendOpFactory* NewFactory = InContext->ManagedObjects->New<UPCGExBlendOpFactory>();
	NewFactory->Priority = Priority;
	NewFactory->Config = Config;
	NewFactory->Config.Init();

	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExBlendOpFactoryProviderSettings::GetDisplayName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExABBlendingType>())
	{
		FString Str = FString::Printf(TEXT("%s %s"), *EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(Config.BlendMode)).ToString(), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandA));

		switch (Config.OutputMode)
		{
		case EPCGExBlendOpOutputMode::SameAsA: break;
		case EPCGExBlendOpOutputMode::SameAsB: if (Config.bUseOperandB) { Str += FString::Printf(TEXT(" ⇌ %s"), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandB)); }
			else { Str += FString::Printf(TEXT(" → %s"), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandB)); }
			break;
		case EPCGExBlendOpOutputMode::New: if (Config.bUseOperandB) { Str += FString::Printf(TEXT(" & %s"), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandB)); }
			else { Str += FString::Printf(TEXT(" → %s"), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OutputTo)); }
			break;
		case EPCGExBlendOpOutputMode::Transient: if (Config.bUseOperandB) { Str += FString::Printf(TEXT(" & %s"), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OperandB)); }
			Str += FString::Printf(TEXT(" ⇢ %s"), *PCGExMetaHelpers::GetSelectorDisplayName(Config.OutputTo));
			break;
		}
		return Str;
	}

	return TEXT("PCGEx | Blend Op");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
