// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertySchemaCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "PCGExPropertyCompiled.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertySchemaCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertySchemaCustomization());
}

FText FPCGExPropertySchemaCustomization::GetHeaderText() const
{
	if (!PropertyHandlePtr.IsValid()) { return FText::FromString(TEXT("None (Unknown)")); }

	// Access schema data directly
	TArray<void*> RawData;
	PropertyHandlePtr.Pin()->AccessRawData(RawData);

	FName PropertyName = NAME_None;
	FString TypeName = TEXT("Unknown");

	if (!RawData.IsEmpty() && RawData[0])
	{
		const FPCGExPropertySchema* Schema = static_cast<FPCGExPropertySchema*>(RawData[0]);
		if (Schema)
		{
			PropertyName = Schema->Name;
			if (const FPCGExPropertyCompiled* Prop = Schema->GetProperty())
			{
				TypeName = Prop->GetTypeName().ToString();
			}
		}
	}

	return FText::FromString(FString::Printf(TEXT("%s (%s)"), *PropertyName.ToString(), *TypeName));
}

void FPCGExPropertySchemaCustomization::OnSchemaChanged()
{
	if (!PropertyHandlePtr.IsValid()) { return; }

	// Access schema to sync
	TArray<void*> RawData;
	PropertyHandlePtr.Pin()->AccessRawData(RawData);
	if (RawData.IsEmpty() || !RawData[0]) { return; }

	FPCGExPropertySchema* Schema = static_cast<FPCGExPropertySchema*>(RawData[0]);
	if (Schema)
	{
		// Sync PropertyName and HeaderId into Property
		Schema->SyncPropertyName();
	}

	// Note: Parent collection will handle ForceRefresh via its own listener
}

bool FPCGExPropertySchemaCustomization::IsReadOnlySchema(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	// Walk up the property hierarchy to find if we're under a property with ReadOnlySchema metadata
	TSharedPtr<IPropertyHandle> ParentHandle = PropertyHandle->GetParentHandle();
	while (ParentHandle.IsValid())
	{
		const FProperty* Property = ParentHandle->GetProperty();
		if (Property && Property->HasMetaData(TEXT("ReadOnlySchema")))
		{
			return true;
		}
		ParentHandle = ParentHandle->GetParentHandle();
	}
	return false;
}

void FPCGExPropertySchemaCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandlePtr = PropertyHandle;
	bIsReadOnly = IsReadOnlySchema(PropertyHandle);

	HeaderRow
		.NameContent()
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertySchemaCustomization::GetHeaderText)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];
}

void FPCGExPropertySchemaCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get Name and Property handles
	TSharedPtr<IPropertyHandle> NameHandle = PropertyHandle->GetChildHandle(TEXT("Name"));
	TSharedPtr<IPropertyHandle> PropertyInnerHandle = PropertyHandle->GetChildHandle(TEXT("Property"));

	if (bIsReadOnly)
	{
		// Read-only mode: Only show the inner Value field from the FInstancedStruct
		// Schema name and type are shown in header, struct type cannot be changed

		if (!PropertyInnerHandle.IsValid()) { return; }

		// Access raw data to get the FInstancedStruct
		TArray<void*> RawData;
		PropertyInnerHandle->AccessRawData(RawData);
		if (RawData.IsEmpty() || !RawData[0]) { return; }

		FInstancedStruct* Instance = static_cast<FInstancedStruct*>(RawData[0]);
		if (!Instance || !Instance->IsValid()) { return; }

		UScriptStruct* InnerStruct = const_cast<UScriptStruct*>(Instance->GetScriptStruct());
		if (!InnerStruct) { return; }

		uint8* StructMemory = Instance->GetMutableMemory();
		if (!StructMemory) { return; }

		// Check if this type should be inlined (simple type)
		const bool bShouldInline = InnerStruct->HasMetaData(TEXT("PCGExInlineValue"));

		// Create FStructOnScope for the inner struct (FPCGExPropertyCompiled_*)
		TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(InnerStruct, StructMemory);

		if (bShouldInline)
		{
			// For simple types, just add the Value property directly
			if (const FProperty* ValueProperty = InnerStruct->FindPropertyByName(TEXT("Value")))
			{
				ChildBuilder.AddExternalStructureProperty(StructOnScope, ValueProperty->GetFName());
			}
		}
		else
		{
			// For complex types, iterate all non-metadata properties
			for (TFieldIterator<FProperty> It(InnerStruct); It; ++It)
			{
				const FProperty* Property = *It;
				if (!Property) { continue; }

				FName PropName = Property->GetFName();

				// Skip metadata properties
				if (PropName == TEXT("PropertyName") || PropName == TEXT("HeaderId") || PropName == TEXT("OutputBuffer"))
					continue;

				ChildBuilder.AddExternalStructureProperty(StructOnScope, PropName);
			}
		}
	}
	else
	{
		// Normal mode: Show Name and Property with full editing capabilities

		// Watch for changes and sync
		if (NameHandle.IsValid())
		{
			NameHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertySchemaCustomization::OnSchemaChanged));
			ChildBuilder.AddProperty(NameHandle.ToSharedRef());
		}

		if (PropertyInnerHandle.IsValid())
		{
			PropertyInnerHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertySchemaCustomization::OnSchemaChanged));
			PropertyInnerHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertySchemaCustomization::OnSchemaChanged));
			ChildBuilder.AddProperty(PropertyInnerHandle.ToSharedRef());
		}
	}
}
