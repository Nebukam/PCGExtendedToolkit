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

void FPCGExPropertySchemaCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandlePtr = PropertyHandle;

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
