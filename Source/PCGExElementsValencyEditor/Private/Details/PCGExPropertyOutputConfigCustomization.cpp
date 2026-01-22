// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyOutputConfigCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyOutputConfigCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyOutputConfigCustomization());
}

void FPCGExPropertyOutputConfigCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> EnabledHandle = PropertyHandle->GetChildHandle(FName("bEnabled"));
	TSharedPtr<IPropertyHandle> PropertyNameHandle = PropertyHandle->GetChildHandle(FName("PropertyName"));
	TSharedPtr<IPropertyHandle> OutputNameHandle = PropertyHandle->GetChildHandle(FName("OutputAttributeName"));

	HeaderRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			// Enable checkbox
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				EnabledHandle->CreatePropertyValueWidget()
			]
			// Property name
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(100)
				[
					PropertyNameHandle->CreatePropertyValueWidget()
				]
			]
		]
		.ValueContent()
		.MinDesiredWidth(200)
		[
			SNew(SHorizontalBox)
			// Arrow
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("\u2192")))  // Right arrow
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FSlateColor(FLinearColor::Gray))
			]
			// Output name
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.MinDesiredWidth(100)
				[
					OutputNameHandle->CreatePropertyValueWidget()
				]
			]
		];
}

void FPCGExPropertyOutputConfigCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// No children - everything is in the header
}
