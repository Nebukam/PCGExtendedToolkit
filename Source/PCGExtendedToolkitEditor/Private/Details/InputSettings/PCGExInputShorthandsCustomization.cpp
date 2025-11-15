// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/InputSettings/PCGExInputShorthandsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PCGExGlobalEditorSettings.h"
#include "PropertyHandle.h"
#include "Collections/PCGExAssetCollection.h"
#include "Constants/PCGExTuple.h"
#include "Details/PCGExCustomizationMacros.h"
#include "Details/Enums/PCGExInlineEnumCustomization.h"
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
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			[
				PCGExEnumCustomization::CreateRadioGroup(InputHandle, FindFirstObjectSafe<UEnum>(*FString(TEXT("EPCGExInputValueType"))))
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
					AttributeHandle->CreatePropertyValueWidget()
				]
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

TSharedRef<IPropertyTypeCustomization> FPCGExInputShorthandVectorCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExInputShorthandVectorCustomization());
}

TSharedRef<SWidget> FPCGExInputShorthandVectorCustomization::CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle)
{
	return PCGEX_VECTORINPUTBOX(ValueHandle);
}

TSharedRef<IPropertyTypeCustomization> FPCGExInputShorthandRotatorCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExInputShorthandRotatorCustomization());
}

TSharedRef<SWidget> FPCGExInputShorthandRotatorCustomization::CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle)
{
	return PCGEX_ROTATORINPUTBOX(ValueHandle);
}
