// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyOverrideEntryCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "PCGExPropertyCompiled.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyOverrideEntryCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyOverrideEntryCustomization());
}

FText FPCGExPropertyOverrideEntryCustomization::GetEntryLabelText() const
{
	if (!PropertyHandlePtr.IsValid()) { return FText::FromString(TEXT("None (Unknown)")); }

	// Access entry data directly - THIS RUNS EACH FRAME, reads fresh data after sync
	TArray<void*> RawData;
	PropertyHandlePtr.Pin()->AccessRawData(RawData);

	FName PropertyName = NAME_None;
	FString TypeName = TEXT("Unknown");

	if (!RawData.IsEmpty() && RawData[0])
	{
		const FPCGExPropertyOverrideEntry* Entry = static_cast<FPCGExPropertyOverrideEntry*>(RawData[0]);
		if (Entry && Entry->Value.IsValid())
		{
			if (const FPCGExPropertyCompiled* Prop = Entry->Value.GetPtr<FPCGExPropertyCompiled>())
			{
				PropertyName = Prop->PropertyName;
				TypeName = Prop->GetTypeName().ToString();
			}
		}
	}

	return FText::FromString(FString::Printf(TEXT("%s (%s)"), *PropertyName.ToString(), *TypeName));
}

void FPCGExPropertyOverrideEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Store property handle for dynamic text
	PropertyHandlePtr = PropertyHandle;

	// Don't set any content on the header to try to hide it completely
	// Everything will be shown in CustomizeChildren
}

void FPCGExPropertyOverrideEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get bEnabled handle for checkbox widget
	TSharedPtr<IPropertyHandle> EnabledHandle = PropertyHandle->GetChildHandle(TEXT("bEnabled"));

	// Get the Value handle (FInstancedStruct)
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));
	if (!ValueHandle.IsValid()) { return; }

	// Access raw data to get the FInstancedStruct
	TArray<void*> RawData;
	ValueHandle->AccessRawData(RawData);
	if (RawData.IsEmpty() || !RawData[0]) { return; }

	FInstancedStruct* Instance = static_cast<FInstancedStruct*>(RawData[0]);
	if (!Instance || !Instance->IsValid()) { return; }

	UScriptStruct* InnerStruct = const_cast<UScriptStruct*>(Instance->GetScriptStruct());
	if (!InnerStruct) { return; }

	uint8* StructMemory = Instance->GetMutableMemory();
	if (!StructMemory) { return; }

	// Check if this type should be inlined
	const bool bShouldInline = InnerStruct->HasMetaData(TEXT("PCGExInlineValue"));

	// Create FStructOnScope for the inner struct (FPCGExPropertyCompiled_*)
	TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(InnerStruct, StructMemory);

	// Find and add the "Value" property
	if (const FProperty* ValueProperty = InnerStruct->FindPropertyByName(TEXT("Value")))
	{
		// Only customize if this is a simple type that should be inlined
		if (bShouldInline)
		{
			IDetailPropertyRow& Row = *ChildBuilder.AddExternalStructureProperty(StructOnScope, ValueProperty->GetFName());

			// Get the property handle for the value widget
			TSharedPtr<IPropertyHandle> ValuePropertyHandle = Row.GetPropertyHandle();

			// Customize the row to show checkbox + label in NameContent and value widget in ValueContent
			Row.CustomWidget()
				.NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4, 0)
					[
						EnabledHandle.IsValid() ? EnabledHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::GetEntryLabelText)))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				.ValueContent()
				[
					ValuePropertyHandle.IsValid() ? ValuePropertyHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
				];
		}
		else
		{
			// Complex type - add a custom row for checkbox + label, then the value property with default expansion
			ChildBuilder.AddCustomRow(FText::GetEmpty())
				.NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4, 0)
					[
						EnabledHandle.IsValid() ? EnabledHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::GetEntryLabelText)))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				];

			// Add the value property normally (will be expandable)
			ChildBuilder.AddExternalStructureProperty(StructOnScope, ValueProperty->GetFName());
		}
	}
}
