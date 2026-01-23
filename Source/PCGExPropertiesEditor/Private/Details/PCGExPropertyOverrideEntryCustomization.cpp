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

void FPCGExPropertyOverrideEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
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

	// Create FStructOnScope for the inner struct (FPCGExPropertyCompiled_*)
	// NOTE: This uses raw memory pointers - they become stale when arrays reorder
	// The solution is to force UI rebuild (via PostEditChangeProperty broadcast) when schema changes
	TSharedRef<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(InnerStruct, StructMemory);

	// Iterate all properties in the inner struct, skip PropertyName
	for (TFieldIterator<FProperty> It(InnerStruct); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property) { continue; }

		FName PropName = Property->GetFName();

		// Skip PropertyName - it's shown in the header
		if (PropName == TEXT("PropertyName")) { continue; }

		// Add value field directly (bypasses FInstancedStruct nesting entirely)
		// This works for all types: simple (Value), Vector (X/Y/Z), Color (R/G/B/A), etc.
		ChildBuilder.AddExternalStructureProperty(StructOnScope, PropName);
	}
}
