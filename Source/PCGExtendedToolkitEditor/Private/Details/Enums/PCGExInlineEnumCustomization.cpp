// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Enums/PCGExInlineEnumCustomization.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

FPCGExInlineEnumCustomization::FPCGExInlineEnumCustomization(const FString& InEnumName)
	: EnumName(InEnumName)
{
}

void FPCGExInlineEnumCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	EnumHandle = PropertyHandle;

	UEnum* Enum = FindFirstObjectSafe<UEnum>(*EnumName);
	if (!Enum) { return; }

	HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()]
		.ValueContent()
		.MaxDesiredWidth(400)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SUniformGridPanel)
				+ SUniformGridPanel::Slot(0, 0)
				[
					GenerateEnumButtons(Enum)
				]
			]
		];
}

void FPCGExInlineEnumCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

TSharedRef<SWidget> FPCGExInlineEnumCustomization::GenerateEnumButtons(UEnum* Enum)
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);

	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
	{
		if (Enum->HasMetaData(TEXT("Hidden"), i)) { continue; }
		const FString KeyName = Enum->GetNameStringByIndex(i);

		FString IconName = Enum->GetMetaData(TEXT("ActionIcon"), i);
		if (IconName.IsEmpty())
		{
			Box->AddSlot().AutoWidth().Padding(2, 2)
			[
				SNew(SButton)
				.Text(Enum->GetDisplayNameTextByIndex(i))
				.ToolTipText(Enum->GetToolTipTextByIndex(i))
				.ButtonColorAndOpacity_Lambda(
					[this, KeyName]
					{
						FString CurrentValue;
						EnumHandle->GetValueAsFormattedString(CurrentValue);
						return CurrentValue == KeyName ? FLinearColor(0.005, 0.005, 0.005, 0.8) : FLinearColor::Transparent;
					})
				.OnClicked_Lambda(
					[this, KeyName]()
					{
						EnumHandle->SetValueFromFormattedString(KeyName);
						return FReply::Handled();
					})
			];
		}
		else
		{
			IconName = TEXT("PCGEx.ActionIcon.") + IconName;
			Box->AddSlot().AutoWidth().Padding(2, 2)
			[
				SNew(SButton)
				.ToolTipText(Enum->GetToolTipTextByIndex(i))
				.ButtonStyle(FAppStyle::Get(), "PCGEx.ActionIcon")
				.ButtonColorAndOpacity_Lambda(
					[this, KeyName]
					{
						FString CurrentValue;
						EnumHandle->GetValueAsFormattedString(CurrentValue);
						return CurrentValue == KeyName ? FLinearColor(0.005, 0.005, 0.005, 0.8f) : FLinearColor::Transparent;
					})
				.OnClicked_Lambda(
					[this, KeyName]()
					{
						EnumHandle->SetValueFromFormattedString(KeyName);
						return FReply::Handled();
					})
				[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush(*IconName))
						.ColorAndOpacity_Lambda(
							[this, Enum, i]
							{
								FString CurrentValue;
								EnumHandle->GetValueAsFormattedString(CurrentValue);
								const FString KeyName = Enum->GetNameStringByIndex(i);
								return (CurrentValue == KeyName)
										   ? FLinearColor::White
										   : FLinearColor::Gray;
							})
					]
			];
		}
	}

	return Box;
}
