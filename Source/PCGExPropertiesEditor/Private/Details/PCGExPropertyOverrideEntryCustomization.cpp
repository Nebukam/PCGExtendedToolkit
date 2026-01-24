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

	// Get bEnabled handle for checkbox widget
	TSharedPtr<IPropertyHandle> EnabledHandle = PropertyHandle->GetChildHandle(TEXT("bEnabled"));

	// Check if this is an inline type
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));
	bool bShouldInline = false;
	if (ValueHandle.IsValid())
	{
		TArray<void*> RawData;
		ValueHandle->AccessRawData(RawData);
		if (!RawData.IsEmpty() && RawData[0])
		{
			FInstancedStruct* Instance = static_cast<FInstancedStruct*>(RawData[0]);
			if (Instance && Instance->IsValid())
			{
				UScriptStruct* InnerStruct = const_cast<UScriptStruct*>(Instance->GetScriptStruct());
				if (InnerStruct)
				{
					bShouldInline = InnerStruct->HasMetaData(TEXT("PCGExInlineValue"));
				}
			}
		}
	}

	// For complex (non-inline) types, show checkbox + label in header
	// For simple (inline) types, header stays empty (everything in CustomizeChildren)
	if (!bShouldInline)
	{
		HeaderRow
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
	}
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

	// Create an attribute that checks if the override is enabled
	TAttribute<bool> IsEnabledAttr = TAttribute<bool>::Create([EnabledHandle]()
	{
		if (EnabledHandle.IsValid())
		{
			bool bEnabled = false;
			EnabledHandle->GetValue(bEnabled);
			return bEnabled;
		}
		return true;
	});

	// Only customize if this is a simple type that should be inlined
	if (bShouldInline)
	{
		// For inline types, only show the "Value" property
		if (const FProperty* ValueProperty = InnerStruct->FindPropertyByName(TEXT("Value")))
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
					SNew(SBox)
					.IsEnabled(IsEnabledAttr)
					[
						ValuePropertyHandle.IsValid() ? ValuePropertyHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
					]
				];
		}
	}
	else
	{
		// Complex type - iterate all non-metadata properties and add them as children
		for (TFieldIterator<FProperty> It(InnerStruct); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property) { continue; }

			FName PropName = Property->GetFName();

			// Skip metadata properties
			if (PropName == TEXT("PropertyName") || PropName == TEXT("HeaderId") || PropName == TEXT("OutputBuffer"))
				continue;

			// Add each property as a child row with enabled state
			IDetailPropertyRow& PropRow = *ChildBuilder.AddExternalStructureProperty(StructOnScope, PropName);
			PropRow.IsEnabled(IsEnabledAttr);
		}
	}
}
