// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Bitmask/PCGExBitmaskRefCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Collections/PCGExBitmaskCollection.h"
#include "Constants/PCGExTuple.h"
#include "Details/PCGExDetailsBitmask.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExBitmaskRefCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExBitmaskRefCustomization());
}

void FPCGExBitmaskRefCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	SourceHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmaskRef, Source));
	IdentifierHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmaskRef, Identifier));
	TSharedPtr<IPropertyHandle> OperationHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmaskRef, Op));

	HeaderRow
		.NameContent()
		[
			SourceHandle->CreatePropertyValueWidget()
		]
		.ValueContent().MinDesiredWidth(400)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(2, 2)
			[
				SAssignNew(ComboBoxWidget, SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&ComboOptions)
				.OnGenerateWidget_Lambda(
					[](TSharedPtr<FName> InItem)
					{
						return SNew(STextBlock).Text(FText::FromName(*InItem));
					})
				.OnSelectionChanged_Lambda(
					[this](TSharedPtr<FName> NewValue, ESelectInfo::Type)
					{
						if (NewValue.IsValid())
						{
							IdentifierHandle->SetValue(*NewValue);
						}
					})
				[
					SNew(STextBlock)
					.Text_Lambda(
						[this]()
						{
							FName CurrentValue;
							IdentifierHandle->GetValue(CurrentValue);
							return FText::FromName(CurrentValue);
						})
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Center)
			.Padding(2, 2)
			[
				OperationHandle->CreatePropertyValueWidget()
			]
		];

	RefreshOptions();

	// Optional: Register callback when Source changes
	SourceHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExBitmaskRefCustomization::RefreshOptions));
}

void FPCGExBitmaskRefCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FPCGExBitmaskRefCustomization::RefreshOptions()
{
	ComboOptions.Empty();

	bool bNoOptions = true;
	UObject* SourceObject = nullptr;
	if (SourceHandle->GetValue(SourceObject) == FPropertyAccess::Success)
	{
		if (UPCGExBitmaskCollection* Collection = Cast<UPCGExBitmaskCollection>(SourceObject))
		{
			TArray<FName> Identifiers = Collection->GetIdentifierOptions();
			for (const FName& Name : Identifiers) { ComboOptions.Add(MakeShared<FName>(Name)); }
			bNoOptions = Identifiers.IsEmpty();
		}
	}

	if (bNoOptions)
	{
		ComboOptions.Add(MakeShared<FName>(FName("{Empty}")));
	}

	if (ComboBoxWidget.IsValid())
	{
		ComboBoxWidget->RefreshOptions();

		// Set initially selected item based on current Identifier value
		FName CurrentValue;
		if (IdentifierHandle->GetValue(CurrentValue) == FPropertyAccess::Success)
		{
			for (auto& Option : ComboOptions)
			{
				if (*Option == CurrentValue)
				{
					ComboBoxWidget->SetSelectedItem(Option);
					break;
				}
			}
		}
	}
}
