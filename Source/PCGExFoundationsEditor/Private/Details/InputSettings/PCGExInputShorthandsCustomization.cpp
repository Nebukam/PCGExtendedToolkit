// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/InputSettings/PCGExInputShorthandsCustomization.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Details/PCGExCustomizationMacros.h"
#include "Details/Enums/PCGExInlineEnumCustomization.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExInputShorthandCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExInputShorthandCustomization());
}

void FPCGExInputShorthandCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get handles
	TSharedPtr<IPropertyHandle> InputHandle = PropertyHandle->GetChildHandle(FName("Input"));
	TSharedPtr<IPropertyHandle> ConstantHandle = PropertyHandle->GetChildHandle(FName("Constant"));
	TSharedPtr<IPropertyHandle> AttributeHandle = PropertyHandle->GetChildHandle(FName("Attribute"));

	HeaderRow.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
			[
				SNew(SBox)
				.Visibility(
					MakeAttributeLambda(
						[InputHandle]()
						{
							uint8 V = 0;
							InputHandle->GetValue(V);
							return V ? EVisibility::Collapsed : EVisibility::Visible;
						}))
				[
					CreateValueWidget(ConstantHandle)
				]
			]
			+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
			[
				SNew(SBox)
				.Visibility(
					MakeAttributeLambda(
						[InputHandle]()
						{
							uint8 V = 0;
							InputHandle->GetValue(V);
							return V ? EVisibility::Visible : EVisibility::Collapsed;
						}))
				[
					CreateAttributeWidget(AttributeHandle)
				]
			]
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			[
				PCGExEnumCustomization::CreateRadioGroup(InputHandle, TEXT("EPCGExInputValueType"))
			]
		];
}

void FPCGExInputShorthandCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

TSharedRef<SWidget> FPCGExInputShorthandCustomization::CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle)
{
	return ValueHandle->CreatePropertyValueWidget();
}

TSharedRef<SWidget> FPCGExInputShorthandCustomization::CreateAttributeWidget(TSharedPtr<IPropertyHandle> AttributeHandle)
{
	FProperty* Prop = AttributeHandle->GetProperty();
	if (CastField<FNameProperty>(Prop) || CastField<FTextProperty>(Prop))
	{
		return AttributeHandle->CreatePropertyValueWidget();
	}

	return SNew(SEditableTextBox)
		.Text_Lambda(
			[AttributeHandle]()
			{
				TArray<void*> RawData;
				AttributeHandle->AccessRawData(RawData);

				if (RawData.Num() > 0)
				{
					FPCGAttributePropertyInputSelector* Selector = static_cast<FPCGAttributePropertyInputSelector*>(RawData[0]);
					if (Selector)
					{
						return FText::FromString(Selector->ToString());
					}
				}
				return FText::GetEmpty();
			})
		.OnTextCommitted_Lambda(
			[AttributeHandle](const FText& NewText, ETextCommit::Type CommitType)
			{
				// Only handle commits from Enter or losing focus
				if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
				{
					TArray<void*> RawData;
					AttributeHandle->AccessRawData(RawData);

					bool bUpdated = false;
					for (void* Ptr : RawData)
					{
						FPCGAttributePropertyInputSelector* Selector = static_cast<FPCGAttributePropertyInputSelector*>(Ptr);
						if (Selector)
						{
							Selector->Update(NewText.ToString());
							bUpdated = true;
						}
					}

					if (bUpdated) { AttributeHandle->NotifyPostChange(EPropertyChangeType::ValueSet); }
				}
			});
}

TSharedRef<IPropertyTypeCustomization> FPCGExInputShorthandVectorCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExInputShorthandVectorCustomization());
}

TSharedRef<SWidget> FPCGExInputShorthandVectorCustomization::CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle)
{
	return PCGEX_VECTORINPUTBOX(ValueHandle);
}

TSharedRef<IPropertyTypeCustomization> FPCGExInputShorthandDirectionCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExInputShorthandDirectionCustomization());
}

void FPCGExInputShorthandDirectionCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get handles
	TSharedPtr<IPropertyHandle> InputHandle = PropertyHandle->GetChildHandle(FName("Input"));
	TSharedPtr<IPropertyHandle> ConstantHandle = PropertyHandle->GetChildHandle(FName("Constant"));
	TSharedPtr<IPropertyHandle> AttributeHandle = PropertyHandle->GetChildHandle(FName("Attribute"));
	TSharedPtr<IPropertyHandle> FlipHandle = PropertyHandle->GetChildHandle(FName("bFlip"));

	HeaderRow.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			[
				PCGExEnumCustomization::CreateRadioGroup(InputHandle, TEXT("EPCGExInputValueType"))
			]
			+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
			[
				SNew(SBox)
				.Visibility(
					MakeAttributeLambda(
						[InputHandle]()
						{
							uint8 V = 0;
							InputHandle->GetValue(V);
							return V ? EVisibility::Collapsed : EVisibility::Visible;
						}))
				[
					CreateValueWidget(ConstantHandle)
				]
			]
			+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
			[
				SNew(SBox)
				.Visibility(
					MakeAttributeLambda(
						[InputHandle]()
						{
							uint8 V = 0;
							InputHandle->GetValue(V);
							return V ? EVisibility::Visible : EVisibility::Collapsed;
						}))
				[
					CreateAttributeWidget(AttributeHandle)
				]
			]
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			[
				FlipHandle->CreatePropertyValueWidget()
			]
		];
}

TSharedRef<IPropertyTypeCustomization> FPCGExInputShorthandRotatorCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExInputShorthandRotatorCustomization());
}

TSharedRef<SWidget> FPCGExInputShorthandRotatorCustomization::CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle)
{
	return PCGEX_ROTATORINPUTBOX(ValueHandle);
}
