﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBranchOnDataAttribute.h"

#include "PCGExBroadcast.h"
#include "PCGExHelpers.h"
#include "Data/PCGExDataHelpers.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGExBranchOnDataAttributeElement"
#define PCGEX_NAMESPACE BranchOnDataAttribute

#if WITH_EDITOR
void UPCGExBranchOnDataAttributeSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (SelectionMode != EPCGExControlFlowSelectionMode::UserDefined)
	{
		InternalBranches.Reset();

		if (const TObjectPtr<UEnum> Enum = GetEnumClass())
		{
			// -1 to bypass the MAX value
			for (int32 Index = 0; Enum && Index < Enum->NumEnums() - 1; ++Index)
			{
				bool bHidden = false;
#if WITH_EDITOR
				// HasMetaData is editor only, so there will be extra pins at runtime, but that should be okay
				bHidden = Enum->HasMetaData(TEXT("Hidden"), Index) || Enum->HasMetaData(TEXT("Spacer"), Index);
#endif // WITH_EDITOR

				if (!bHidden)
				{
					FPCGExBranchOnDataPin& Pin = InternalBranches.Emplace_GetRef(SelectionMode == EPCGExControlFlowSelectionMode::EnumInteger);
					Pin.StringValue = Enum->GetDisplayNameTextByIndex(Index).BuildSourceString();
					Pin.Label = FName(Pin.StringValue);
					Pin.Check = SelectionMode == EPCGExControlFlowSelectionMode::EnumInteger ? EPCGExComparisonDataType::Numeric : EPCGExComparisonDataType::String;
					Pin.NumericValue = Enum->GetValueByIndex(Index);
					Pin.NumericCompare = EPCGExComparison::StrictlyEqual;
					Pin.StringCompare = EPCGExStringComparison::StrictlyEqual;
				}
			}
		}
	}
	else
	{
		InternalBranches.Reset(Branches.Num());
		for (FPCGExBranchOnDataPin& Pin : Branches) { InternalBranches.Add(Pin); }
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
	MarkPackageDirty();
}
#endif

TArray<FPCGPinProperties> UPCGExBranchOnDataAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(GetMainInputPin(), "Inputs", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBranchOnDataAttributeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	PCGEX_PIN_ANY(GetMainOutputPin(), "Default output -- Any collection that couldn't be dispatched to an output pin will end up here.", Normal)
	for (const FPCGExBranchOnDataPin& OutPin : InternalBranches) { PinProperties.Emplace(OutPin.Label); }

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BranchOnDataAttribute)

TObjectPtr<UEnum> UPCGExBranchOnDataAttributeSettings::GetEnumClass() const
{
	if (EnumSource == EPCGExEnumConstantSourceType::Picker) { return EnumClass; }
	return EnumPicker.Class;
}

bool FPCGExBranchOnDataAttributeElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BranchOnDataAttribute)

	PCGEX_VALIDATE_NAME(Settings->BranchSource)
	Context->Dispatch.Init(0, Settings->InternalBranches.Num());

	return true;
}

bool FPCGExBranchOnDataAttributeElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBranchOnDataAttributeElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BranchOnDataAttribute)
	PCGEX_EXECUTION_CHECK

	const int32 NumBranches = Settings->InternalBranches.Num();
	
	PCGEX_ON_INITIAL_EXECUTION
	{
		FPCGAttributePropertyInputSelector DummySelector;
		DummySelector.Update(Settings->BranchSource.ToString());

		FPCGAttributeIdentifier ReadIdentifier;
		ReadIdentifier.Name = DummySelector.GetAttributeName();
		ReadIdentifier.MetadataDomain = PCGMetadataDomainID::Data;

		TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(Settings->GetMainInputPin());

		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			if (!TaggedData.Data || !TaggedData.Data->Metadata) { continue; }
			const FPCGMetadataAttributeBase* Attr = nullptr;

			if (PCGEx::HasAttribute(TaggedData.Data, ReadIdentifier)) { Attr = TaggedData.Data->Metadata->GetConstAttribute(ReadIdentifier); }

			FName OutputPin = Settings->GetMainOutputPin();
			bool bDistributed = false;

			if (!Attr)
			{
				PCGEX_LOG_INVALID_ATTR_C(Context, Branch Source, Settings->BranchSource)
			}
			else
			{
				PCGEx::ExecuteWithRightType(
					Attr->GetTypeId(), [&](auto ValueType)
					{
						using T_ATTR = decltype(ValueType);
						const FPCGMetadataAttribute<T_ATTR>* TypedAtt = static_cast<const FPCGMetadataAttribute<T_ATTR>*>(Attr);

						T_ATTR Value = PCGExDataHelpers::ReadDataValue(TypedAtt);

						const double AsNumeric = PCGEx::Convert<T_ATTR, double>(Value);
						const FString AsString = PCGEx::Convert<T_ATTR, FString>(Value);

						for (int i = 0; i < NumBranches; ++i)
						{
							const FPCGExBranchOnDataPin& Pin = Settings->InternalBranches[i];
							
							if (Pin.Check == EPCGExComparisonDataType::Numeric) { bDistributed = PCGExCompare::Compare(Pin.NumericCompare, static_cast<double>(Pin.NumericValue), AsNumeric, Pin.Tolerance); }
							else { bDistributed = PCGExCompare::Compare(Pin.StringCompare, Pin.StringValue, AsString); }

							if (bDistributed)
							{
								OutputPin = Pin.Label;
								Context->Dispatch[i]++;
								break;
							}
						}
					});
			}

			Context->StageOutput(
				const_cast<UPCGData*>(TaggedData.Data.Get()), OutputPin,
				TaggedData.Tags, false, false, false);
		}
	}

	for (int i = 0; i < NumBranches; i++) { if (!Context->Dispatch[i]) { Context->OutputData.InactiveOutputPinBitmask |= 1ULL << (i+1); } }
	
	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
