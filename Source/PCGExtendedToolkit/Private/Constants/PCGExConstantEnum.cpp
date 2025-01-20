// Copyright 2025 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Constants/PCGExConstantEnum.h"

#include "PCGComponent.h"
#include "PCGExCompare.h"

#if WITH_EDITOR
FString UPCGExConstantEnumSettings::GetDisplayName() const
{
	const FName Name = GetEnumName();
	if (Name.IsNone()) { return TEXT("..."); }

	if (OutputMode == EPCGExEnumOutputMode::EEOM_Single)
	{
		return Name.ToString() +
			"::" +
			SelectedEnum.Class->GetDisplayNameTextByValue(SelectedEnum.Value).BuildSourceString() +
			" (" + FString::FromInt(SelectedEnum.Value) + ")";
	}

	return Name.ToString();
}
#endif

// Adapted from similar handling in PCGSwitch.cpp
void UPCGExConstantEnumSettings::PostLoad()
{
	Super::PostLoad();

	CachePinLabels();
	if (EnabledExportValues.Num() == 0)
	{
		FillEnabledExportValues();
	}

#if WITH_EDITOR
	if (UPCGNode* OuterNode = Cast<UPCGNode>(GetOuter()))
	{
		TArray<UPCGPin*> SerializedOutputPins = OuterNode->GetOutputPins();

		if (SerializedOutputPins.Num() == CachedPinLabels.Num())
		{
			for (int32 i = 0; i < CachedPinLabels.Num(); ++i)
			{
				if (SerializedOutputPins[i] && SerializedOutputPins[i]->Properties.Label != CachedPinLabels[i])
				{
					OuterNode->RenameOutputPin(SerializedOutputPins[i]->Properties.Label, CachedPinLabels[i], /*bBroadcastUpdate=*/false);
				}
			}
		}
	}
#endif
}

#if WITH_EDITOR
// Adapted from similar handling in PCGSwitch.cpp
void UPCGExConstantEnumSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName Prop = PropertyChangedEvent.GetMemberPropertyName();

	if (Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, SelectedEnum) ||
		Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputMode) ||
		Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputType))
	{
		CachePinLabels();
	}

	if (Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, SelectedEnum))
	{
		if (SelectedEnum.Class)
		{
			FillEnabledExportValues();
		}
	}
}
#endif

void UPCGExConstantEnumSettings::OnOverrideSettingsDuplicatedInternal(bool bSkippedPostLoad)
{
	Super::OnOverrideSettingsDuplicatedInternal(bSkippedPostLoad);
	if (bSkippedPostLoad)
	{
		CachePinLabels();
		if (EnabledExportValues.Num() == 0)
		{
			FillEnabledExportValues();
		}
	}
}

void UPCGExConstantEnumSettings::FillEnabledExportValues()
{
	EnabledExportValues.Empty();
	for (const auto Value : GetEnumValueMap())
	{
		EnabledExportValues.Emplace(Value.Get<1>(), true);
	}
}

void UPCGExConstantEnumSettings::CachePinLabels()
{
	CachedPinLabels.Empty();
	Algo::Transform(
		OutputPinProperties(), CachedPinLabels, [](const FPCGPinProperties& Property)
		{
			return Property.Label;
		});
}

TArray<PCGExConstantEnumConstants::FMapping> UPCGExConstantEnumSettings::GetEnumValueMap() const
{
	// Note: arguably this should be <FName, FString, int64>, but:
	// - pin properties expect a name rather than a string
	// - the formatting in the table view is weird if you have a name next to a string
	// - PCG Switch behaves like this
	// ...so we're going to convert the description into a name and hope there aren't any emojis

	TArray<PCGExConstantEnumConstants::FMapping> Out;
	if (!SelectedEnum.Class)
	{
		return Out;
	}

	// Adapted from PCGControlFlow.cpp
	// -1 to bypass the MAX value
	for (int32 Index = 0; SelectedEnum.Class && Index < SelectedEnum.Class->NumEnums() - 1; ++Index)
	{
		bool bHidden = false;
#if WITH_EDITOR
		bHidden = SelectedEnum.Class->HasMetaData(TEXT("Hidden"), Index) || SelectedEnum.Class->HasMetaData(TEXT("Spacer"), Index);
#endif

		if (!bHidden)
		{
			Out.Push(
				{
					StripEnumNamespaceFromKey ? // Key
						FName(SelectedEnum.Class->GetNameStringByIndex(Index)) :
						SelectedEnum.Class->GetNameByIndex(Index),
					FName(SelectedEnum.Class->GetDisplayNameTextByIndex(Index).BuildSourceString()), // Description
					SelectedEnum.Class->GetValueByIndex(Index),                                      // Value
					Index                                                                            // Index
				});
		}
	}

	return Out;
}

FName UPCGExConstantEnumSettings::GetEnumName() const
{
	if (SelectedEnum.Class)
	{
		return FName(SelectedEnum.Class->GetName());
	}
	return FName("");
}

#if WITH_EDITOR
EPCGChangeType UPCGExConstantEnumSettings::GetChangeTypeForProperty(const FName& PropName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(PropName);

	if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, bEnabled) ||
		PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, SelectedEnum) ||
		PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputMode) ||
		PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputType))
	{
		ChangeType |= EPCGChangeType::Structural;
	}

	return ChangeType;
}
#endif

TArray<FPCGPinProperties> UPCGExConstantEnumSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	if (!SelectedEnum.Class) { return PinProperties; }
	const auto EnumName = GetEnumName();

	FText ToolTip = FText::GetEmpty();

	// It's this or a lambda, and a lambda is probably overkill 
#define MAKE_TOOLTIP_FOR_VALUE(_KEY, _VALUE) FText::FromString(_KEY.ToString() + " (" + FString::FromInt(_VALUE) + ")");

	switch (OutputMode)
	{
	case EPCGExEnumOutputMode::EEOM_Single:
		ToolTip = MAKE_TOOLTIP_FOR_VALUE(SelectedEnum.Class->GetNameByValue(SelectedEnum.Value), SelectedEnum.Value);
		PinProperties.Emplace(PCGExConstantEnumConstants::SingleOutputPinName, EPCGDataType::Param, true, false, ToolTip);
		break;

	case EPCGExEnumOutputMode::EEOM_All:
	case EPCGExEnumOutputMode::EEOM_Selection:
		ToolTip = FText::FromName(EnumName);
		PinProperties.Emplace(PCGExConstantEnumConstants::SingleOutputPinName, EPCGDataType::Param, true, false, ToolTip);
		break;

	case EPCGExEnumOutputMode::EEOM_SelectionToMultiplePins:
		for (const PCGExConstantEnumConstants::FMapping& Mapping : GetEnumValueMap())
		{
			if (EnabledExportValues.Contains(Mapping.Get<1>()) && *EnabledExportValues.Find(Mapping.Get<1>()))
			{
				ToolTip = MAKE_TOOLTIP_FOR_VALUE(Mapping.Get<0>(), Mapping.Get<2>());
				PinProperties.Emplace(Mapping.Get<1>(), EPCGDataType::Param, true, false, ToolTip);
			}
		}
		break;

	case EPCGExEnumOutputMode::EEOM_AllToMultiplePins:
	default:
		// -1 to bypass the MAX value
		for (const PCGExConstantEnumConstants::FMapping& Mapping : GetEnumValueMap())
		{
			ToolTip = MAKE_TOOLTIP_FOR_VALUE(Mapping.Get<0>(), Mapping.Get<2>());
			PinProperties.Emplace(Mapping.Get<1>(), EPCGDataType::Param, true, false, ToolTip);
		}
	}

	// Output bitmask last
	if (bOutputFlags) { PCGEX_PIN_PARAM(PCGExConstantEnumConstants::BitflagOutputPinName, "Flags representing the current selection within the enum", Required, {}); }

	return PinProperties;
#undef MAKE_TOOLTIP_FOR_VALUE
}

FPCGElementPtr UPCGExConstantEnumSettings::CreateElement() const
{
	return MakeShared<FPCGExConstantEnumElement>();
}

bool FPCGExConstantEnumElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(ConstantEnum)

	auto ValidateNames = [&]()-> bool
	{
		// Validating names, will throw an error
		// This is in an lambda because the macro returns false, which would have the node run forever
		if (Settings->OutputEnumKeys) { PCGEX_VALIDATE_NAME(Settings->KeyAttribute) }
		if (Settings->OutputEnumDescriptions) { PCGEX_VALIDATE_NAME(Settings->DescriptionAttribute) }
		if (Settings->OutputEnumValues) { PCGEX_VALIDATE_NAME(Settings->ValueOutputAttribute) }
		if (Settings->bOutputFlags) { PCGEX_VALIDATE_NAME(Settings->ValueOutputAttribute) }
		return true;
	};

	if (!ValidateNames()) { return true; }

	// No class selected, so can't output anything
	if (!Settings->SelectedEnum.Class)
	{
		return true;
	}

	// No data selected to output
	if (!Settings->OutputEnumValues && !Settings->OutputEnumKeys && !Settings->OutputEnumDescriptions)
	{
		return true;
	}

	const TArray<PCGExConstantEnumConstants::FMapping> Unfiltered = Settings->GetEnumValueMap();
	TArray<PCGExConstantEnumConstants::FMapping> Filtered;

	FPCGExBitmask Bitflags;
	Bitflags.Mode = EPCGExBitmaskMode::Individual;
	Bitflags.Bits.SetNum(Unfiltered.Num());
	for (int i = 0; i < Unfiltered.Num(); i++)
	{
		Bitflags.Bits[i].BitIndex = Settings->FlagBitOffset + i;
		Bitflags.Bits[i].bValue = false;
	}


	switch (Settings->OutputMode)
	{
	// Just output the one selected
	case EPCGExEnumOutputMode::EEOM_Single:
		{
			for (int i = 0; i < Unfiltered.Num(); i++)
			{
				if (Unfiltered[i].Get<2>() == Settings->SelectedEnum.Value)
				{
					// Using the single pin so connections don't break when the user changes the value
					StageEnumValuesSinglePin(Context, Settings, {Unfiltered[i]}, Bitflags);
					break;
				}
			}
		}
		break;

	// Output everything
	case EPCGExEnumOutputMode::EEOM_All:
		StageEnumValuesSinglePin(Context, Settings, Unfiltered, Bitflags);
		break;

	case EPCGExEnumOutputMode::EEOM_Selection:
	case EPCGExEnumOutputMode::EEOM_SelectionToMultiplePins:
		Filtered = Settings->GetEnumValueMap().FilterByPredicate(
			[&](const PCGExConstantEnumConstants::FMapping& V)
			{
				return Settings->EnabledExportValues.Contains(V.Get<1>()) && *Settings->EnabledExportValues.Find(V.Get<1>());
			});

		if (Settings->OutputMode == EPCGExEnumOutputMode::EEOM_Selection)
		{
			StageEnumValuesSinglePin(Context, Settings, Filtered, Bitflags);
		}
		else
		{
			StageEnumValuesSeparatePins(Context, Settings, Filtered, Bitflags);
		}

		break;

	// Output everything, but on different pins
	case EPCGExEnumOutputMode::EEOM_AllToMultiplePins:
	default:
		StageEnumValuesSeparatePins(Context, Settings, Unfiltered, Bitflags);
	}

	StageBitFlags(Context, Settings, Bitflags);

	Context->Done();
	return Context->TryComplete();
}

void FPCGExConstantEnumElement::StageEnumValuesSeparatePins(
	FPCGExContext* InContext,
	const UPCGExConstantEnumSettings* Settings,
	const TArray<PCGExConstantEnumConstants::FMapping>& ValueData,
	FPCGExBitmask& OutBitflags)
{
	for (const PCGExConstantEnumConstants::FMapping& T : ValueData)
	{
		OutBitflags.Bits[T.Get<3>()].bValue = true;

		UPCGParamData* OutputData = InContext->ManagedObjects->New<UPCGParamData>();

		FPCGMetadataAttribute<FName>* KeyAttrib = nullptr;
		FPCGMetadataAttribute<FName>* DescriptionAttrib = nullptr;
		FPCGMetadataAttribute<int64>* ValueAttrib = nullptr;

		if (Settings->OutputEnumKeys)
		{
			KeyAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->KeyAttribute, NAME_None, false, false);
		}

		if (Settings->OutputEnumDescriptions)
		{
			DescriptionAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->DescriptionAttribute, NAME_None, false, false);
		}

		if (Settings->OutputEnumValues)
		{
			ValueAttrib = OutputData->Metadata->CreateAttribute<int64>(Settings->ValueOutputAttribute, 0, true, false);
		}

		const auto Entry = OutputData->Metadata->AddEntry();
		if (KeyAttrib)
		{
			KeyAttrib->SetValue(Entry, T.Get<0>());
		}
		if (DescriptionAttrib)
		{
			DescriptionAttrib->SetValue(Entry, T.Get<1>());
		}
		if (ValueAttrib)
		{
			ValueAttrib->SetValue(Entry, T.Get<2>());
		}

		InContext->StageOutput(T.Get<1>(), OutputData, true);
	}
}

void FPCGExConstantEnumElement::StageEnumValuesSinglePin(
	FPCGExContext* InContext,
	const UPCGExConstantEnumSettings* Settings,
	const TArray<PCGExConstantEnumConstants::FMapping>& ValueData,
	FPCGExBitmask& OutBitflags)
{
	UPCGParamData* OutputData = InContext->ManagedObjects->New<UPCGParamData>();

	FPCGMetadataAttribute<FName>* KeyAttrib = nullptr;
	FPCGMetadataAttribute<int64>* ValueAttrib = nullptr;
	FPCGMetadataAttribute<FName>* DescriptionAttrib = nullptr;


	if (Settings->OutputEnumKeys)
	{
		KeyAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->KeyAttribute, NAME_None, false, false);
	}

	if (Settings->OutputEnumDescriptions)
	{
		DescriptionAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->DescriptionAttribute, NAME_None, false, false);
	}

	if (Settings->OutputEnumValues)
	{
		ValueAttrib = OutputData->Metadata->CreateAttribute<int64>(Settings->ValueOutputAttribute, 0, true, false);
	}

	for (const PCGExConstantEnumConstants::FMapping& T : ValueData)
	{
		OutBitflags.Bits[T.Get<3>()].bValue = true;

		const auto Entry = OutputData->Metadata->AddEntry();
		if (KeyAttrib)
		{
			KeyAttrib->SetValue(Entry, T.Get<0>());
		}
		if (DescriptionAttrib)
		{
			DescriptionAttrib->SetValue(Entry, T.Get<1>());
		}
		if (ValueAttrib)
		{
			ValueAttrib->SetValue(Entry, T.Get<2>());
		}
	}

	InContext->StageOutput(PCGExConstantEnumConstants::SingleOutputPinName, OutputData, true);
}

void FPCGExConstantEnumElement::StageBitFlags(FPCGExContext* InContext, const UPCGExConstantEnumSettings* Settings, FPCGExBitmask& OutBitflags)
{
	if (!Settings->bOutputFlags) { return; }

	UPCGParamData* OutputData = InContext->ManagedObjects->New<UPCGParamData>();
	OutputData->Metadata->CreateAttribute<int64>(Settings->FlagsName, OutBitflags.Get(), false, false);
	OutputData->Metadata->AddEntry();
	InContext->StageOutput(PCGExConstantEnumConstants::BitflagOutputPinName, OutputData, true);
}

FPCGContext* FPCGExConstantEnumElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExContext* Context = new FPCGExContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}
