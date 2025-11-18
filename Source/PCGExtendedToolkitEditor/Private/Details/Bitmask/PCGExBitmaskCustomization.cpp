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
#include "Misc/Filters/PCGExBitmaskFilter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"

namespace PCGExBitmaskCustomization
{
	void FillGrid(TSharedRef<SUniformGridPanel> Grid, TSharedPtr<IPropertyHandle> BitmaskHandle)
	{
		for (int32 i = 0; i < 64; ++i)
		{
			Grid->AddSlot(i % 8, i / 8)
			[
				SNew(SCheckBox)
				.RenderOpacity((i / 8) % 2 != 0 ? 0.8 : 1)
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
	}
}

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
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> Handle = PropertyHandle->GetChildHandle(i);
		if (Handle->GetProperty()->GetFName() == FName("Bitmask")) { BuildGrid(PropertyHandle, ChildBuilder); }
		else { ChildBuilder.AddProperty(Handle.ToSharedRef()); }
	}
}

void FPCGExBitmaskCustomization::BuildGrid(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder) const
{
	TSharedPtr<IPropertyHandle> ModeHandle = PropertyHandle->GetChildHandle(FName("Mode"));
	TSharedPtr<IPropertyHandle> BitmaskHandle = PropertyHandle->GetChildHandle(FName("Bitmask"));

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel);
	Grid->SetSlotPadding(FMargin(2, 2));

	PCGExBitmaskCustomization::FillGrid(Grid, BitmaskHandle);

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
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Bitmask :"))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(10)
			]
			+ SHorizontalBox::Slot().Padding(1).MinWidth(100)
			.VAlign(VAlign_Center)
			[
				BitmaskHandle->CreatePropertyValueWidget()
			]
		]
		.ValueContent()
		[
			Grid
		];
}

TSharedRef<IPropertyTypeCustomization> FPCGExBitmaskWithOperationCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExBitmaskWithOperationCustomization());
}

TSharedRef<IPropertyTypeCustomization> FPCGExBitmaskFilterConfigCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExBitmaskFilterConfigCustomization());
}

void FPCGExBitmaskFilterConfigCustomization::BuildGrid(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder) const
{
	TSharedPtr<IPropertyHandle> BitmaskHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmaskFilterConfig, Bitmask));
	TSharedPtr<IPropertyHandle> InputHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExBitmaskFilterConfig, MaskInput));

	TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel);
	Grid->SetSlotPadding(FMargin(2, 2));

	PCGExBitmaskCustomization::FillGrid(Grid, BitmaskHandle);

	// Create a grid of [bool], the grid itself will only be visible in composite view
	ChildBuilder
		.AddCustomRow(FText::FromString("Bitmask"))
		.Visibility(
			MakeAttributeLambda(
				[InputHandle]()
				{
					uint8 EnumValue = 0;
					InputHandle->GetValue(EnumValue);
					return EnumValue == 0 ? EVisibility::Visible : EVisibility::Collapsed;
				}))
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Bitmask :"))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(10)
			]
			+ SHorizontalBox::Slot().Padding(1).MinWidth(100)
			.VAlign(VAlign_Center)
			[
				BitmaskHandle->CreatePropertyValueWidget()
			]
		]
		.ValueContent()
		//.MinDesiredWidth(400)
		[
			Grid
		];
}
