// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyOverridesCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "PCGExPropertyCompiled.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Delegates/Delegate.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyOverridesCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyOverridesCustomization());
}

void FPCGExPropertyOverridesCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Store utilities for refresh
	WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	// Store property handle for dynamic text
	PropertyHandlePtr = PropertyHandle;

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverridesCustomization::GetHeaderText)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
		];
}

FText FPCGExPropertyOverridesCustomization::GetHeaderText() const
{
	if (!PropertyHandlePtr.IsValid()) { return FText::FromString(TEXT("0 / 0 active")); }

	// Get raw access to count enabled
	TArray<void*> RawData;
	PropertyHandlePtr.Pin()->AccessRawData(RawData);
	int32 EnabledCount = 0;
	int32 TotalCount = 0;
	if (!RawData.IsEmpty() && RawData[0])
	{
		const FPCGExPropertyOverrides* OverridesStruct = static_cast<FPCGExPropertyOverrides*>(RawData[0]);
		TotalCount = OverridesStruct->Overrides.Num();
		EnabledCount = OverridesStruct->GetEnabledCount();
	}

	return FText::FromString(FString::Printf(TEXT("%d / %d active"), EnabledCount, TotalCount));
}

void FPCGExPropertyOverridesCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get the Overrides array handle
	TSharedPtr<IPropertyHandle> OverridesArrayHandle = PropertyHandle->GetChildHandle(TEXT("Overrides"));
	if (!OverridesArrayHandle.IsValid()) { return; }

	// Watch for array changes to force refresh
	auto RefreshDelegate = FSimpleDelegate::CreateLambda([this]()
	{
		if (TSharedPtr<IPropertyUtilities> PropertyUtilities = WeakPropertyUtilities.Pin())
		{
			// Force complete rebuild of customizations when array changes
			// This recreates FStructOnScope instances with fresh pointers
			PropertyUtilities->ForceRefresh();
		}
	});

	// Handles add/remove/reorder
	OverridesArrayHandle->SetOnPropertyValueChanged(RefreshDelegate);

	// Handles value changes within entries (like bEnabled toggle or value edits)
	OverridesArrayHandle->SetOnChildPropertyValueChanged(RefreshDelegate);

	// Hide array controls (add/remove/reorder buttons) - manually iterate instead
	uint32 NumElements = 0;
	OverridesArrayHandle->GetNumChildren(NumElements);

	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedPtr<IPropertyHandle> ElementHandle = OverridesArrayHandle->GetChildHandle(i);
		if (ElementHandle.IsValid())
		{
			// Add each entry - FPCGExPropertyOverrideEntryCustomization will handle display
			IDetailPropertyRow& Row = ChildBuilder.AddProperty(ElementHandle.ToSharedRef());
			Row.ShowPropertyButtons(false); // Hide reset/browse buttons
		}
	}
}
