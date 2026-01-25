// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertySchemaCollectionCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "PCGExPropertyCompiled.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertySchemaCollectionCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertySchemaCollectionCustomization());
}

void FPCGExPropertySchemaCollectionCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Store utilities and handles
	WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	PropertyHandlePtr = PropertyHandle;

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertySchemaCollectionCustomization::GetHeaderText)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
		];
}

FText FPCGExPropertySchemaCollectionCustomization::GetHeaderText() const
{
	if (!PropertyHandlePtr.IsValid()) { return FText::FromString(TEXT("0 properties")); }

	// Get raw access to count schemas
	TArray<void*> RawData;
	PropertyHandlePtr.Pin()->AccessRawData(RawData);
	int32 SchemaCount = 0;
	if (!RawData.IsEmpty() && RawData[0])
	{
		const FPCGExPropertySchemaCollection* Collection = static_cast<FPCGExPropertySchemaCollection*>(RawData[0]);
		SchemaCount = Collection->Schemas.Num();
	}

	return FText::FromString(FString::Printf(TEXT("%d %s"), SchemaCount, SchemaCount == 1 ? TEXT("property") : TEXT("properties")));
}

void FPCGExPropertySchemaCollectionCustomization::OnSchemasArrayChanged()
{
	if (!PropertyHandlePtr.IsValid()) { return; }

	// Access the collection to sync schemas
	TArray<void*> RawData;
	PropertyHandlePtr.Pin()->AccessRawData(RawData);
	if (RawData.IsEmpty() || !RawData[0]) { return; }

	FPCGExPropertySchemaCollection* Collection = static_cast<FPCGExPropertySchemaCollection*>(RawData[0]);

	// Sync PropertyName and HeaderId for all schemas
	for (FPCGExPropertySchema& Schema : Collection->Schemas)
	{
		Schema.SyncPropertyName();
	}

	// Force complete UI rebuild to refresh all dependent customizations
	if (TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
	{
		PropertyUtilities->ForceRefresh();
	}
}

void FPCGExPropertySchemaCollectionCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get the Schemas array handle
	TSharedPtr<IPropertyHandle> SchemasArrayHandle = PropertyHandle->GetChildHandle(TEXT("Schemas"));
	if (!SchemasArrayHandle.IsValid()) { return; }

	SchemasArrayHandlePtr = SchemasArrayHandle;

	// Watch for array changes and trigger sync + refresh
	SchemasArrayHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertySchemaCollectionCustomization::OnSchemasArrayChanged));
	SchemasArrayHandle->SetOnChildPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertySchemaCollectionCustomization::OnSchemasArrayChanged));

	// Display the Schemas array normally
	ChildBuilder.AddProperty(SchemasArrayHandle.ToSharedRef());
}
