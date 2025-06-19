// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExReduceDataAttribute.h"

#define LOCTEXT_NAMESPACE "PCGExReduceDataAttributeElement"
#define PCGEX_NAMESPACE ReduceDataAttribute

TArray<FPCGPinProperties> UPCGExReduceDataAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(GetMainInputPin(), "Inputs", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExReduceDataAttributeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(GetMainOutputPin(), "Reduced attribute.", Normal, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ReduceDataAttribute)

bool FPCGExReduceDataAttributeElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ReduceDataAttribute)

	FPCGAttributeIdentifier ReadIdentifier = Settings->Attributes.GetSourceSelector().GetAttributeName();
	PCGEX_VALIDATE_NAME(ReadIdentifier.Name)
	ReadIdentifier.MetadataDomain = PCGMetadataDomainID::Data;

	Context->WriteIdentifier = Settings->Attributes.GetTargetSelector().GetAttributeName();
	PCGEX_VALIDATE_NAME(Context->WriteIdentifier.Name)
	Context->WriteIdentifier.MetadataDomain = PCGMetadataDomainID::Elements;

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(Settings->GetMainInputPin());

	TMap<int16, int32> TypeCounter;

	int32 MaxCount = 0;
	Context->Attributes.Reserve(Inputs.Num());
	TypeCounter.Reserve(Inputs.Num());

	for (const FPCGTaggedData& TaggedData : Inputs)
	{
		if (!TaggedData.Data || !TaggedData.Data->Metadata) { continue; }
		const FPCGMetadataAttributeBase* Attribute = nullptr;

		if (PCGEx::HasAttribute(TaggedData.Data, ReadIdentifier)) { Attribute = TaggedData.Data->Metadata->GetConstAttribute(ReadIdentifier); }

		if (!Attribute)
		{
			if (!Settings->bQuietMissingAttribute)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Some data are missing the source attribute."));
			}

			continue;
		}

		int32& Count = TypeCounter.FindOrAdd(Attribute->GetTypeId(), 0);
		Count++;

		if (MaxCount < Count)
		{
			MaxCount = Count;
			Context->OutputType = static_cast<EPCGMetadataTypes>(Attribute->GetTypeId());
		}

		Context->Attributes.Add(Attribute);
	}

	if (Context->Attributes.IsEmpty())
	{
		if (!Settings->bQuietMissingInputError)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing any valid input."));
		}

		return false;
	}

	if (Settings->Method == EPCGExReduceDataDomainMethod::Join)
	{
		Context->OutputType = EPCGMetadataTypes::String;
	}

	return true;
}

bool FPCGExReduceDataAttributeElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExReduceDataAttributeElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ReduceDataAttribute)
	PCGEX_EXECUTION_CHECK

	UPCGParamData* ParamData = Context->ManagedObjects->New<UPCGParamData>();

	PCGEX_ON_INITIAL_EXECUTION
	{
		UPCGMetadata* OutMetadata = ParamData->Metadata;

		if (Settings->Method == EPCGExReduceDataDomainMethod::Join)
		{
			TArray<FString> StringsToJoin;
			StringsToJoin.Reserve(Context->Attributes.Num());
			FString OutValue = TEXT("");

			if (Settings->bOutputType)
			{
				for (int i = 0; i < Context->Attributes.Num(); i++)
				{
					const FPCGMetadataAttributeBase* Att = Context->Attributes[i];
					PCGEx::ExecuteWithRightType(
						Att->GetTypeId(), [&](auto ValueType)
						{
							using T_ATTR = decltype(ValueType);
							const FPCGMetadataAttribute<T_ATTR>* TypedAtt = static_cast<const FPCGMetadataAttribute<T_ATTR>*>(Att);
							T_ATTR Value = PCGEX_READ_DATA_ENTRY(TypedAtt);

							PCGEx::ExecuteWithRightType(
								Settings->OutputType, [&](auto DummyValue)
								{
									using T = decltype(DummyValue);
									StringsToJoin.Add(PCGEx::Convert<FString>(PCGEx::Convert<T>(Value)));
								});
						});
				}
			}
			else
			{
				for (int i = 0; i < Context->Attributes.Num(); i++)
				{
					const FPCGMetadataAttributeBase* Att = Context->Attributes[i];
					PCGEx::ExecuteWithRightType(
						Att->GetTypeId(), [&](auto ValueType)
						{
							using T_ATTR = decltype(ValueType);
							const FPCGMetadataAttribute<T_ATTR>* TypedAtt = static_cast<const FPCGMetadataAttribute<T_ATTR>*>(Att);
							T_ATTR Value = PCGEX_READ_DATA_ENTRY(TypedAtt);
							StringsToJoin.Add(PCGEx::Convert<FString>(Value));
						});
				}
			}

			OutValue = FString::Join(StringsToJoin, *Settings->JoinDelimiter);
			FPCGMetadataAttribute<FString>* OutAtt = OutMetadata->FindOrCreateAttribute(Context->WriteIdentifier, OutValue);
			OutAtt->SetValue(PCGFirstEntryKey, OutValue);
		}
		else
		{
			PCGEx::ExecuteWithRightType(
				Context->OutputType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					T ReducedValue = T{};
					for (int i = 0; i < Context->Attributes.Num(); i++)
					{
						const FPCGMetadataAttributeBase* Att = Context->Attributes[i];
						PCGEx::ExecuteWithRightType(
							Att->GetTypeId(), [&](auto ValueType)
							{
								using T_ATTR = decltype(ValueType);
								const FPCGMetadataAttribute<T_ATTR>* TypedAtt = static_cast<const FPCGMetadataAttribute<T_ATTR>*>(Att);
								T_ATTR Value = PCGEX_READ_DATA_ENTRY(TypedAtt);

								switch (Settings->Method)
								{
								case EPCGExReduceDataDomainMethod::Min:
									if (i == 0) { ReducedValue = PCGEx::Convert<T>(ReducedValue); }
									else { ReducedValue = PCGExBlend::Min(ReducedValue, PCGEx::Convert<T>(ReducedValue)); }
									break;
								case EPCGExReduceDataDomainMethod::Max:
									if (i == 0) { ReducedValue = PCGEx::Convert<T>(ReducedValue); }
									else { ReducedValue = PCGExBlend::Max(ReducedValue, PCGEx::Convert<T>(ReducedValue)); }
									break;
								case EPCGExReduceDataDomainMethod::Sum:
									ReducedValue = PCGExBlend::Add(ReducedValue, PCGEx::Convert<T>(ReducedValue));
									break;
								case EPCGExReduceDataDomainMethod::Average:
									ReducedValue = PCGExBlend::Add(ReducedValue, PCGEx::Convert<T>(ReducedValue));
									break;
								default:
								case EPCGExReduceDataDomainMethod::Join:
									break;
								}
							});
					}

					if (Settings->Method == EPCGExReduceDataDomainMethod::Average)
					{
						ReducedValue = PCGExBlend::Div(ReducedValue, Context->Attributes.Num());
					}

					FPCGMetadataAttribute<T>* OutAtt = OutMetadata->FindOrCreateAttribute(Context->WriteIdentifier, ReducedValue);
					OutAtt->SetValue(PCGFirstEntryKey, ReducedValue);
				});
		}
	}

	Context->StageOutput(ParamData, true, true);
	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
