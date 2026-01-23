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

	// Get raw access to count enabled
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	int32 EnabledCount = 0;
	int32 TotalCount = 0;
	if (!RawData.IsEmpty() && RawData[0])
	{
		const FPCGExPropertyOverrides* OverridesStruct = static_cast<FPCGExPropertyOverrides*>(RawData[0]);
		TotalCount = OverridesStruct->Overrides.Num();
		EnabledCount = OverridesStruct->GetEnabledCount();
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%d / %d active"), EnabledCount, TotalCount)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
		];
}

void FPCGExPropertyOverridesCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Just add the Overrides array as a child - each entry will be customized by FPCGExPropertyOverrideEntryCustomization
	TSharedPtr<IPropertyHandle> OverridesArrayHandle = PropertyHandle->GetChildHandle(TEXT("Overrides"));
	if (OverridesArrayHandle.IsValid())
	{
		ChildBuilder.AddProperty(OverridesArrayHandle.ToSharedRef());
	}
}
