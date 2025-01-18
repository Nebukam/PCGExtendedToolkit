#include "PCGExConstantEnum.h"

// Adapted from similar handling in PCGSwitch.cpp
void UPCGExConstantEnumSettings::PostLoad() {
	Super::PostLoad();

	CachePinLabels();
	
	#if WITH_EDITOR
	if (UPCGNode* OuterNode = Cast<UPCGNode>(GetOuter())) {
		TArray<UPCGPin*> SerializedOutputPins = OuterNode->GetOutputPins();
		
		if (SerializedOutputPins.Num() == CachedPinLabels.Num()) {
			for (int32 i = 0; i < CachedPinLabels.Num(); ++i) {
				if (SerializedOutputPins[i] && SerializedOutputPins[i]->Properties.Label != CachedPinLabels[i]) {
					OuterNode->RenameOutputPin(SerializedOutputPins[i]->Properties.Label, CachedPinLabels[i], /*bBroadcastUpdate=*/false);
				}
			}
		}
	}
	#endif
}

// Adapted from similar handling in PCGSwitch.cpp
void UPCGExConstantEnumSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) {
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName Prop = PropertyChangedEvent.GetMemberPropertyName();
	
	if (Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, SelectedEnum) ||
		Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputMode) ||
		Prop == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputType))
		{
		CachePinLabels();
	}
}

void UPCGExConstantEnumSettings::OnOverrideSettingsDuplicatedInternal(bool bSkippedPostLoad) {
	Super::OnOverrideSettingsDuplicatedInternal(bSkippedPostLoad);
	if (bSkippedPostLoad) {
		CachePinLabels();
	}
}

void UPCGExConstantEnumSettings::CachePinLabels() {
	CachedPinLabels.Empty();
	Algo::Transform(OutputPinProperties(), CachedPinLabels, [](const FPCGPinProperties& Property)
	{
		return Property.Label;
	});
}

TArray<TTuple<FName, FName, int64>> UPCGExConstantEnumSettings::GetEnumValueMap() const {
	// Note: arguably this should be <FName, FString, int64>, but:
	// - pin properties expect a name rather than a string
	// - the formatting in the table view is weird if you have a name next to a string
	// - PCG Switch behaves like this
	// ...so we're going to convert the description into a name and hope there aren't any emojis
	
	TArray<TTuple<FName, FName, int64>> Out;
	if (!SelectedEnum.Class) {
		return Out;
	}
	
	// Adapted from PCGControlFlow.cpp
	// -1 to bypass the MAX value
	for (int32 Index = 0; SelectedEnum.Class && Index < SelectedEnum.Class->NumEnums() - 1; ++Index) {
		bool bHidden = false;
		#if WITH_EDITOR
		bHidden = SelectedEnum.Class->HasMetaData(TEXT("Hidden"), Index) || SelectedEnum.Class->HasMetaData(TEXT("Spacer"), Index);
		#endif 

		if (!bHidden) {
			Out.Push({
				StripEnumNamespaceFromKey ? // Key
					FName(SelectedEnum.Class->GetNameStringByIndex(Index)) :
					SelectedEnum.Class->GetNameByIndex(Index), 
				FName(SelectedEnum.Class->GetDisplayNameTextByIndex(Index).BuildSourceString()), // Description
				SelectedEnum.Class->GetValueByIndex(Index) // Value
			});
		}
	}

	return Out;
}

FName UPCGExConstantEnumSettings::GetEnumName() const {
	if (SelectedEnum.Class) return FName(SelectedEnum.Class->GetName());
	return FName("");
}

EPCGChangeType UPCGExConstantEnumSettings::GetChangeTypeForProperty(const FName& PropName) const {
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(PropName);

	if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, bEnabled) ||
		PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, SelectedEnum) ||
		PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputMode) ||
		PropName == GET_MEMBER_NAME_CHECKED(UPCGExConstantEnumSettings, OutputType)) {

		ChangeType |= EPCGChangeType::Structural;
	}

	return ChangeType;
}

TArray<FPCGPinProperties> UPCGExConstantEnumSettings::OutputPinProperties() const {

	TArray<FPCGPinProperties> Out;

	if (!SelectedEnum.Class) return Out;
	const auto EnumName = GetEnumName();

	FText ToolTip = FText::GetEmpty();

	// It's this or a lambda, and a lambda is probably overkill 
	#define MAKE_TOOLTIP_FOR_VALUE(_KEY, _VALUE) FText::FromString(_KEY.ToString() + " (" + FString::FromInt(_VALUE) + ")"); 
	
	switch (OutputMode) {
		case EPCGExEnumOutputMode::EEOM_Single:
			ToolTip = MAKE_TOOLTIP_FOR_VALUE(SelectedEnum.Class->GetNameByValue(SelectedEnum.Value), SelectedEnum.Value);
			Out.Emplace(PCGExConstantEnumConstants::SingleOutputPinName, EPCGDataType::Param, true, false, ToolTip);
			break;

		case EPCGExEnumOutputMode::EEOM_All:
			ToolTip = FText::FromName(EnumName);
			Out.Emplace(PCGExConstantEnumConstants::SingleOutputPinName, EPCGDataType::Param, true, false, ToolTip);
			break;

		case EPCGExEnumOutputMode::EEOM_Selection: // TODO
		case EPCGExEnumOutputMode::EEOM_SelectionToMultiplePins: // TODO
			break;

		case EPCGExEnumOutputMode::EEOM_AllToMultiplePins:
		default:
			// -1 to bypass the MAX value
			for (const TTuple<FName, FName, int64>& Mapping: GetEnumValueMap()) {
				ToolTip = MAKE_TOOLTIP_FOR_VALUE(Mapping.Get<0>(), Mapping.Get<2>());
				Out.Emplace(Mapping.Get<1>(), EPCGDataType::Param, true, false, ToolTip);
			}
	}
	
	return Out;
	#undef MAKE_TOOLTIP_FOR_VALUE
}

FPCGElementPtr UPCGExConstantEnumSettings::CreateElement() const {
	return MakeShared<FPCGExConstantEnumElement>();
}

bool FPCGExConstantEnumElement::ExecuteInternal(FPCGContext* InContext) const {
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(ConstantEnum)

	// No class selected, so can't output anything
	if (!Settings->SelectedEnum.Class) return true;

	// No data selected to output
	if (!Settings->OutputEnumValues && !Settings->OutputEnumKeys && !Settings->OutputEnumDescriptions) return true;
	
	FName Key;
	FName Description;
	int64 Value;
	
	switch (Settings->OutputMode) {

	// Just output the one selected
	case EPCGExEnumOutputMode::EEOM_Single:
		Value = Settings->SelectedEnum.Value;
		Key = Settings->SelectedEnum.Class->GetNameByValue(Value);
		Description = FName(Settings->SelectedEnum.Class->GetDisplayNameTextByValue(Value).BuildSourceString());

		// Using the single pin so connections don't break when the user changes the value
		StageEnumValuesSinglePin(Context, Settings, { {Key, Description, Value } });
		break;

	// Output everything
	case EPCGExEnumOutputMode::EEOM_All:
		StageEnumValuesSinglePin(Context, Settings, Settings->GetEnumValueMap());
		break;

	// Output everything, but on different pins
	case EPCGExEnumOutputMode::EEOM_AllToMultiplePins:
	default:
		StageEnumValuesSeparatePins(Context, Settings, Settings->GetEnumValueMap());
		
	}

	Context->Done();
	return Context->TryComplete();
}

void FPCGExConstantEnumElement::StageEnumValuesSeparatePins(
	FPCGExContext* InContext,
	const UPCGExConstantEnumSettings* Settings,
	const TArray<TTuple<FName, FName, int64>>& ValueData
) {

	for (const TTuple<FName, FName, int64>& T : ValueData) {
		UPCGParamData* OutputData = InContext->ManagedObjects->New<UPCGParamData>();
		
		FPCGMetadataAttribute<FName>* KeyAttrib = nullptr;
		FPCGMetadataAttribute<FName>* DescriptionAttrib = nullptr;
		FPCGMetadataAttribute<int64>* ValueAttrib = nullptr;
		
		if (Settings->OutputEnumKeys) {
			KeyAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->KeyAttribute, NAME_None, false, false);
		}
		
		if (Settings->OutputEnumDescriptions) {
			DescriptionAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->DescriptionAttribute, NAME_None, false, false);
		}

		if (Settings->OutputEnumValues) {
			ValueAttrib = OutputData->Metadata->CreateAttribute<int64>(PCGExConstantEnumConstants::ValueOutputAttribute, 0, true, false);
		}
		
		const auto Entry = OutputData->Metadata->AddEntry();
		if (KeyAttrib) KeyAttrib->SetValue(Entry, T.Get<0>());
		if (DescriptionAttrib) DescriptionAttrib->SetValue(Entry, T.Get<1>());
		if (ValueAttrib) ValueAttrib->SetValue(Entry, T.Get<2>());

		InContext->StageOutput(T.Get<1>(), OutputData, true);
	}
	
}

void FPCGExConstantEnumElement::StageEnumValuesSinglePin(FPCGExContext* InContext,
                                                         const UPCGExConstantEnumSettings* Settings, const TArray<TTuple<FName, FName, int64>>& ValueData) {
	UPCGParamData* OutputData = InContext->ManagedObjects->New<UPCGParamData>();
		
	FPCGMetadataAttribute<FName>* KeyAttrib = nullptr;
	FPCGMetadataAttribute<int64>* ValueAttrib = nullptr;
	FPCGMetadataAttribute<FName>* DescriptionAttrib = nullptr;

	if (Settings->OutputEnumKeys) {
		KeyAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->KeyAttribute, NAME_None, false, false);
	}
	
	if (Settings->OutputEnumDescriptions) {
		DescriptionAttrib = OutputData->Metadata->CreateAttribute<FName>(Settings->DescriptionAttribute, NAME_None, false, false);
	}

	if (Settings->OutputEnumValues) {
		ValueAttrib = OutputData->Metadata->CreateAttribute<int64>(PCGExConstantEnumConstants::ValueOutputAttribute, 0, true, false);
	}
		
	for (const TTuple<FName, FName, int64>& T : ValueData) {
		const auto Entry = OutputData->Metadata->AddEntry();
		if (KeyAttrib) KeyAttrib->SetValue(Entry, T.Get<0>());
		if (DescriptionAttrib) DescriptionAttrib->SetValue(Entry, T.Get<1>());
		if (ValueAttrib) ValueAttrib->SetValue(Entry, T.Get<2>());
	}

	InContext->StageOutput(PCGExConstantEnumConstants::SingleOutputPinName, OutputData, true);
}

FPCGContext* FPCGExConstantEnumElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) {
	FPCGExContext* Context = new FPCGExContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	return Context;
}
