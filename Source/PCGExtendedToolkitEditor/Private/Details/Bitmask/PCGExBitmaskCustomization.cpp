// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Bitmask/PCGExBitmaskCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Constants/PCGExTuple.h"
#include "Details/PCGExDetailsBitmask.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

TSharedRef<IPropertyTypeCustomization> FPCGExBitmaskCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExBitmaskCustomization());
}

void FPCGExBitmaskCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
}

void FPCGExBitmaskCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmask, Mode));
	TSharedPtr<IPropertyHandle> BitmaskHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmask, Bitmask));
	TSharedPtr<IPropertyHandle> MutationsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmask, Mutations));

	// Add regular properties, those will be managed as usual
	ChildBuilder.AddProperty(ModeHandle.ToSharedRef());
	ChildBuilder.AddProperty(BitmaskHandle.ToSharedRef());
	ChildBuilder.AddProperty(MutationsHandle.ToSharedRef());

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel);
	Grid->SetSlotPadding(FMargin(2, 2));

	for (int32 i = 0; i < 64; ++i)
	{
		Grid->AddSlot(i % 8, i / 8)
		[
			SNew(SCheckBox)
			.RenderOpacity(i % 8 == 0 ? 0.7 : 1)
			.IsChecked_Lambda(
				[BitmaskHandle, BitIndex = i]()
				{
					int64 Mask = 0;
					BitmaskHandle->GetValue(Mask);
					return (Mask & (1LL << BitIndex)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
			.OnCheckStateChanged_Lambda(
				[BitmaskHandle, BitIndex = i](ECheckBoxState NewState)
				{
					int64 Mask = 0;
					BitmaskHandle->GetValue(Mask);

					const int64 Bit = (1LL << BitIndex);
					if (NewState == ECheckBoxState::Checked) { Mask |= Bit; }
					else { Mask &= ~Bit; }

					BitmaskHandle->SetValue(Mask);
				})
		];
	}

	// Create a grid of [bool], the grid itself will only be visible in composite view
	ChildBuilder
		.AddCustomRow(FText::FromString("Bitmask"))
		.Visibility(
			MakeAttributeLambda(
				[ModeHandle]()
				{
					uint8 EnumValue = 0;
					ModeHandle->GetValue(EnumValue);
					return EnumValue != 1 ? EVisibility::Visible : EVisibility::Collapsed;
				}))
		.NameContent()
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("Bits"))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(10)
		]
		.ValueContent()
		//.MinDesiredWidth(400)
		[
			Grid
		];
}
