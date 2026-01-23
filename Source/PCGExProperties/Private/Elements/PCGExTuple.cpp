// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExTuple.h"

#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Containers/PCGExManagedObjects.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "PCGExPropertyTypes.h"

#if WITH_EDITOR
#include "UObject/UObjectGlobals.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Tuple

#if WITH_EDITOR
void UPCGExTupleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExTupleSettings::PostEditChangeProperty);

	bool bNeedsSync = false;
	bool bNeedsUIRefresh = false;

	if (PropertyChangedEvent.MemberProperty)
	{
		FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		EPropertyChangeType::Type ChangeType = PropertyChangedEvent.ChangeType;

		// Check for ANY changes in Composition
		if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExTupleSettings, Composition))
		{
			bNeedsSync = true;
			bNeedsUIRefresh = true;
		}
		// Also catch changes to Composition array elements (e.g., changing Property type or Name)
		else if (PropertyChangedEvent.MemberProperty->GetOwnerStruct() == FPCGExPropertySchema::StaticStruct())
		{
			bNeedsSync = true;
			bNeedsUIRefresh = true;
		}
		// Check for structural values change
		else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExTupleSettings, Values) && (ChangeType == EPropertyChangeType::ArrayAdd || ChangeType == EPropertyChangeType::ArrayRemove || ChangeType == EPropertyChangeType::ArrayClear || ChangeType == EPropertyChangeType::ArrayMove))
		{
			bNeedsSync = true;
		}
	}

	if (!bNeedsSync && !bNeedsUIRefresh)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return; // Skip processing
	}

	// Build schema array from composition (only if we need to sync)
	if (bNeedsSync)
	{
		// CRITICAL: Sync PropertyName and HeaderId into Property before building schema
		// This ensures overrides get the correct PropertyName and HeaderId when synced
		for (FPCGExPropertySchema& SchemaEntry : Composition.Schemas)
		{
			SchemaEntry.SyncPropertyName();
		}

		// Build schema and sync all rows
		TArray<FInstancedStruct> Schema = Composition.BuildSchema();
		for (FPCGExPropertyOverrides& Row : Values)
		{
			Row.SyncToSchema(Schema);
		}
	}

	(void)MarkPackageDirty();

	// Force UI refresh BEFORE Super - this ensures details panel rebuilds customizations
	if (bNeedsUIRefresh)
	{
		// Mark Values as changed to force full customization rebuild
		FProperty* ValuesProperty = FindFProperty<FProperty>(GetClass(), TEXT("Values"));
		if (ValuesProperty)
		{
			// Use ArrayClear type to force aggressive rebuild
			FPropertyChangedEvent RefreshEvent(ValuesProperty, EPropertyChangeType::ArrayClear);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(this, RefreshEvent);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

TArray<FPCGPinProperties> UPCGExTupleSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExTupleSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_PARAM(FName("Tuple"), TEXT("Tuple."), Required)
	return PinProperties;
}

FPCGElementPtr UPCGExTupleSettings::CreateElement() const { return MakeShared<FPCGExTupleElement>(); }


bool FPCGExTupleElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Tuple)

	UPCGParamData* TupleData = Context->ManagedObjects->New<UPCGParamData>();

	TArray<FPCGMetadataAttributeBase*> Attributes;
	TArray<int64> Keys;

	Attributes.Reserve(Settings->Composition.Num());
	Keys.Reserve(Settings->Composition.Num());

	// Create attributes from schema
	for (const FPCGExPropertySchema& SchemaEntry : Settings->Composition.Schemas)
	{
		// Check for existing attribute
		const FPCGMetadataAttributeBase* ExistingAttr = TupleData->Metadata->GetConstAttribute(SchemaEntry.Name);
		if (ExistingAttr)
		{
			PCGEX_LOG_INVALID_ATTR_C(Context, Header Name, SchemaEntry.Name)
			Attributes.Add(nullptr);
			continue;
		}

		const FPCGExPropertyCompiled* Property = SchemaEntry.GetProperty();
		if (!Property)
		{
			Attributes.Add(nullptr);
			continue;
		}

		Attributes.Add(Property->CreateMetadataAttribute(TupleData->Metadata, SchemaEntry.Name));
	}

	// Create all keys
	for (int i = 0; i < Settings->Values.Num(); ++i) { Keys.Add(TupleData->Metadata->AddEntry()); }

	// Write values to attributes (only enabled overrides)
	for (int i = 0; i < Settings->Composition.Num(); ++i)
	{
		FPCGMetadataAttributeBase* Attribute = Attributes[i];
		if (!Attribute) { continue; }

		for (int k = 0; k < Keys.Num(); k++)
		{
			const FPCGExPropertyOverrides& Row = Settings->Values[k];

			// Only write if this column is enabled in this row
			if (!Row.IsOverrideEnabled(i)) { continue; }

			if (const FPCGExPropertyCompiled* Property = Row.Overrides[i].GetProperty())
			{
				Property->WriteMetadataValue(Attribute, Keys[k]);
			}
		}
	}

	TSet<FString> Tags;
	PCGExArrayHelpers::AppendEntriesFromCommaSeparatedList(Settings->CommaSeparatedTags, Tags);
	Context->StageOutput(TupleData, FName("Tuple"), PCGExData::EStaging::None, Tags);

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
