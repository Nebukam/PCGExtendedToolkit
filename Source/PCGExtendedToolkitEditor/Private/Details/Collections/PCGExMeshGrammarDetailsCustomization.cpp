// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExMeshGrammarDetailsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PCGExGlobalEditorSettings.h"
#include "PropertyHandle.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExMeshGrammar.h"
#include "Constants/PCGExTuple.h"
#include "Details/PCGExCustomizationMacros.h"
#include "Details/Enums/PCGExInlineEnumCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"

#define PCGEX_SMALL_LABEL(_TEXT) \
+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(1, 0)\
[SNew(STextBlock).Text(FText::FromString(TEXT(_TEXT))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray)).MinDesiredWidth(10)]

#define PCGEX_SMALL_LABEL_COL(_TEXT, _COL) \
+ SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Center).Padding(1,8,1,2)\
[SNew(STextBlock).Text(FText::FromString(TEXT(_TEXT))).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(_COL)).MinDesiredWidth(10)]

#define PCGEX_SEP_LABEL(_TEXT)\
+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0)\
[SNew(STextBlock).Text(FText::FromString(_TEXT)).Font(IDetailLayoutBuilder::GetDetailFont()).ColorAndOpacity(FSlateColor(FLinearColor::Gray))]

#define PCGEX_FIELD_BASE(_HANDLE, _TYPE, _PART, _TOOLTIP)\
+ SHorizontalBox::Slot().Padding(1)[\
SNew(SNumericEntryBox<double>).Value_Lambda([=]() -> TOptional<double>{_TYPE V; _HANDLE->GetValue(V); return V._PART;})\
.OnValueCommitted_Lambda([=](double NewVal, ETextCommit::Type){_TYPE V; _HANDLE->GetValue(V); V._PART = NewVal; _HANDLE->SetValue(V);})\
.ToolTipText(FString(_TOOLTIP).IsEmpty() ? _HANDLE->GetToolTipText() : FText::FromString(_TOOLTIP)).AllowSpin(true)

#define PCGEX_STEP_VISIBILITY(_HANDLE)\
.Visibility_Lambda([_HANDLE](){uint8 EnumValue = 0;\
if (_HANDLE->GetValue(EnumValue) == FPropertyAccess::Success){ return EnumValue ? EVisibility::Visible : EVisibility::Collapsed;}\
return EVisibility::Collapsed;})

#define PCGEX_FIELD(_HANDLE, _TYPE, _PART, _TOOLTIP) PCGEX_FIELD_BASE(_HANDLE, _TYPE, _PART, _TOOLTIP)]

TSharedRef<IPropertyTypeCustomization> FPCGExMeshGrammarDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExMeshGrammarDetailsCustomization());
}

void FPCGExMeshGrammarDetailsCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> SymbolHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshGrammarDetails, Symbol));
	TSharedPtr<IPropertyHandle> ScaleModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshGrammarDetails, ScaleMode));
	TSharedPtr<IPropertyHandle> SizeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshGrammarDetails, Size));
	TSharedPtr<IPropertyHandle> DebugColorHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExMeshGrammarDetails, DebugColor));

	// Grab parent collection
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (UPCGExMeshCollection* Collection = !OuterObjects.IsEmpty() ? Cast<UPCGExMeshCollection>(OuterObjects[0]) : nullptr;
		Collection && !PropertyHandle->GetProperty()->GetFName().ToString().Contains(TEXT("Global")))
	{
		HeaderRow.NameContent()[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(1).AutoWidth()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot().Padding(10, 0).FillWidth(1).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.Text_Lambda(
					[Collection]()
					{
						return Collection->GlobalGrammarMode == EPCGExGlobalVariationRule::Overrule
							       ? FText::FromString(TEXT("⚠ Overruled"))
							       : FText::GetEmpty();
					})
				.ColorAndOpacity_Lambda(
					[Collection]()
					{
						return Collection->GlobalGrammarMode == EPCGExGlobalVariationRule::Overrule
							       ? FLinearColor(1.0f, 0.5f, 0.1f, 0.5)
							       : FLinearColor::Transparent;
					})
			]

		];
	}
	else
	{
		HeaderRow.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
	}

	HeaderRow.ValueContent()
	         .MinDesiredWidth(400)
	[
		// Scale source
		SNew(SHorizontalBox)
		PCGEX_SMALL_LABEL("Symbol")
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
		[
			SymbolHandle->CreatePropertyValueWidget()
		]
		// Scale mode
		+ SHorizontalBox::Slot().Padding(1).AutoWidth()
		[
			//ScaleModeHandle->CreatePropertyValueWidget()
			PCGExEnumCustomization::CreateRadioGroup(ScaleModeHandle, TEXT("EPCGExGrammarScaleMode"))
		]
		// Size
		PCGEX_SMALL_LABEL("·· Size")
		+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
		[
			SizeHandle->CreatePropertyValueWidget()
		]
		// Debug color
		PCGEX_SMALL_LABEL("·· ")
		+ SHorizontalBox::Slot().Padding(1).MaxWidth(25)
		[
			DebugColorHandle->CreatePropertyValueWidget()
		]
	];
}

void FPCGExMeshGrammarDetailsCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

#undef PCGEX_SMALL_LABEL
#undef PCGEX_SEP_LABEL
#undef PCGEX_FIELD_BASE
#undef PCGEX_FIELD
#undef PCGEX_FIELD_D
#undef PCGEX_STEP_VISIBILITY
