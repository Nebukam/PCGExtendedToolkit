// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExFittingVariationsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PCGExGlobalEditorSettings.h"
#include "PropertyHandle.h"
#include "Collections/PCGExAssetCollection.h"
#include "Constants/PCGExTuple.h"
#include "Details/Enums/PCGExInlineEnumCustomization.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExFittingVariationsCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExFittingVariationsCustomization());
}

void FPCGExFittingVariationsCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Grab parent collection
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);

	if (UPCGExAssetCollection* Collection = !OuterObjects.IsEmpty() ? Cast<UPCGExAssetCollection>(OuterObjects[0]) : nullptr)
	{
		HeaderRow.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.Text_Lambda(
					[Collection]()
					{
						return Collection->GlobalVariationMode == EPCGExGlobalVariationRule::Overrule
							       ? FText::FromString("Overruled in collection settings!")
							       : FText::GetEmpty();
					})
				.ColorAndOpacity_Lambda(
					[Collection]()
					{
						return Collection->GlobalVariationMode == EPCGExGlobalVariationRule::Overrule
							       ? FLinearColor(1.0f, 0.5f, 0.1f, 0.25)
							       : FLinearColor::Transparent;
					})
			];
	}
	else
	{
		HeaderRow.NameContent()[

			PropertyHandle->CreatePropertyNameWidget()
		];
	}
}

void FPCGExFittingVariationsCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
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

#define PCGEX_VECTORINPUTBOX(_HANDLE)\
SNew(SVectorInputBox)\
.X_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("X"))->GetValue(Val); return Val; })\
.Y_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Y"))->GetValue(Val); return Val; })\
.Z_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Z"))->GetValue(Val); return Val; })\
.OnXCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("X"))->SetValue(Val); })\
.OnYCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Y"))->SetValue(Val); })\
.OnZCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Z"))->SetValue(Val); })
	
#define PCGEX_ROTATORINPUTBOX(_HANDLE)\
SNew(SRotatorInputBox)\
.Roll_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Roll"))->GetValue(Val); return Val; })\
.Pitch_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Pitch"))->GetValue(Val); return Val; })\
.Yaw_Lambda([_HANDLE] { double Val; _HANDLE->GetChildHandle(TEXT("Yaw"))->GetValue(Val); return Val; })\
.OnRollCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Roll"))->SetValue(Val); })\
.OnPitchCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Pitch"))->SetValue(Val); })\
.OnYawCommitted_Lambda([_HANDLE](double Val, ETextCommit::Type) { _HANDLE->GetChildHandle(TEXT("Yaw"))->SetValue(Val); })

#define PCGEX_FIELD(_HANDLE, _TYPE, _PART, _TOOLTIP) PCGEX_FIELD_BASE(_HANDLE, _TYPE, _PART, _TOOLTIP)]

#pragma region Offset Min/Max

	// Get handles
	TSharedPtr<IPropertyHandle> OffsetMinHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, OffsetMin));
	TSharedPtr<IPropertyHandle> OffsetMaxHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, OffsetMax));
	TSharedPtr<IPropertyHandle> AbsoluteOffsetHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, bAbsoluteOffset));
	TSharedPtr<IPropertyHandle> OffsetSteppingModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, OffsetStepping));
	TSharedPtr<IPropertyHandle> OffsetStepsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, OffsetSteps));

	// Add custom Offset row
	ChildBuilder
		.AddCustomRow(FText::FromString("Offset"))
		.Visibility(TAttribute<EVisibility>::Create([]() { return GetDefault<UPCGExGlobalEditorSettings>()->GetPropertyVisibility(FName("VariationOffset")); }))
		.NameContent()
		[
			SNew(SVerticalBox)
			PCGEX_SMALL_LABEL_COL("Offset Min:Max", FLinearColor::White)
			+ SVerticalBox::Slot() // First line: toggle
			.AutoHeight()
			.Padding(0, 2, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					PCGExEnumCustomization::CreateRadioGroup(OffsetSteppingModeHandle, FindFirstObjectSafe<UEnum>(*FString(TEXT("EPCGExStepping"))))
				]
				PCGEX_SMALL_LABEL("·· Absolute Space : ")
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					AbsoluteOffsetHandle->CreatePropertyValueWidget()
				]
			]
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot() // X/Y/Z
			.Padding(0, 2, 0, 2)
			.AutoHeight()
			[
				// X pair
				SNew(SHorizontalBox)
				PCGEX_SMALL_LABEL(" X")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(OffsetMinHandle, FVector, X, "Min X") PCGEX_SEP_LABEL(":") PCGEX_FIELD(OffsetMaxHandle, FVector, X, "Max X")
				]
				// Y pair
				PCGEX_SMALL_LABEL("·· Y")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(OffsetMinHandle, FVector, Y, "Min Y") PCGEX_SEP_LABEL(":") PCGEX_FIELD(OffsetMaxHandle, FVector, Y, "Max Y")
				]
				// Z pair
				PCGEX_SMALL_LABEL("·· Z")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(OffsetMinHandle, FVector, Z, "Min Z") PCGEX_SEP_LABEL(":") PCGEX_FIELD(OffsetMaxHandle, FVector, Z, "Max Z")
				]
			]
			+ SVerticalBox::Slot() // Step
			.Padding(0, 0, 0, 2)
			.AutoHeight()
			[
				// X pair
				SNew(SHorizontalBox)
				PCGEX_STEP_VISIBILITY(OffsetSteppingModeHandle)
				PCGEX_SMALL_LABEL(" Steps : ")
				+ SHorizontalBox::Slot().FillWidth(1).Padding(1).VAlign(VAlign_Center)
				[
					PCGEX_VECTORINPUTBOX(OffsetStepsHandle)
				]
			]
		];

#pragma endregion

#pragma region Rotation Min/Max


	// Get handles
	TSharedPtr<IPropertyHandle> RotationMinHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, RotationMin));
	TSharedPtr<IPropertyHandle> RotationMaxHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, RotationMax));
	TSharedPtr<IPropertyHandle> AbsoluteRotHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, AbsoluteRotation));
	TSharedPtr<IPropertyHandle> RotationSteppingModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, RotationStepping));
	TSharedPtr<IPropertyHandle> RotationStepsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, RotationSteps));
	
	// Add custom Offset row
	ChildBuilder
		.AddCustomRow(FText::FromString("Rotation"))
		.Visibility(TAttribute<EVisibility>::Create([]() { return GetDefault<UPCGExGlobalEditorSettings>()->GetPropertyVisibility(FName("VariationRotation")); }))
		.NameContent()
		[
			SNew(SVerticalBox)
			PCGEX_SMALL_LABEL_COL("Rotation Min:Max", FLinearColor::White)
			+ SVerticalBox::Slot() // First line: toggle
			.AutoHeight()
			.Padding(0, 2, 0, 8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					PCGExEnumCustomization::CreateRadioGroup(RotationSteppingModeHandle, FindFirstObjectSafe<UEnum>(*FString(TEXT("EPCGExStepping"))))
				]
				PCGEX_SMALL_LABEL("·· Absolute Rotation : ")
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					PCGExEnumCustomization::CreateCheckboxGroup(AbsoluteRotHandle, FindFirstObjectSafe<UEnum>(*FString(TEXT("EPCGExAbsoluteRotationFlags"))), {})
				]
			]
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot() // Second line: X/Y/Z
			.Padding(0, 2, 0, 2)
			.AutoHeight()
			[
				// X pair
				SNew(SHorizontalBox)
				PCGEX_SMALL_LABEL(" R")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(RotationMinHandle, FRotator, Roll, "Min Roll") PCGEX_SEP_LABEL(":") PCGEX_FIELD(RotationMaxHandle, FRotator, Roll, "Max Roll")
				]
				// Y pair
				PCGEX_SMALL_LABEL("·· P")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(RotationMinHandle, FRotator, Pitch, "Min Pitch") PCGEX_SEP_LABEL(":") PCGEX_FIELD(RotationMaxHandle, FRotator, Pitch, "Max Pitch")
				]
				// Z pair
				PCGEX_SMALL_LABEL("·· Y")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(RotationMinHandle, FRotator, Yaw, "Min Yaw") PCGEX_SEP_LABEL(":") PCGEX_FIELD(RotationMaxHandle, FRotator, Yaw, "Max Yaw")
				]
			]
			+ SVerticalBox::Slot() // Step
			.Padding(0, 0, 0, 2)
			.AutoHeight()
			[
				// X pair
				SNew(SHorizontalBox)
				PCGEX_STEP_VISIBILITY(RotationSteppingModeHandle)
				PCGEX_SMALL_LABEL(" Steps : ")
				+ SHorizontalBox::Slot().FillWidth(1).Padding(1).VAlign(VAlign_Center)
				[
					PCGEX_ROTATORINPUTBOX(RotationStepsHandle)
				]
			]
		];

#pragma endregion

#pragma region Scale Min/Max

	// Get handles
	TSharedPtr<IPropertyHandle> ScaleMinHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, ScaleMin));
	TSharedPtr<IPropertyHandle> ScaleMaxHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, ScaleMax));
	TSharedPtr<IPropertyHandle> UniformScaleHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, bUniformScale));
	TSharedPtr<IPropertyHandle> ScaleSteppingModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, ScaleStepping));
	TSharedPtr<IPropertyHandle> ScaleStepsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, ScaleSteps));
	
#define PCGEX_UNIFORM_VISIBILITY .IsEnabled_Lambda([UniformScaleHandle]() { bool bUniform = false; UniformScaleHandle->GetValue(bUniform); return !bUniform; }) ]

	// Add custom Offset row
	ChildBuilder
		.AddCustomRow(FText::FromString("Scale"))
		.Visibility(TAttribute<EVisibility>::Create([]() { return GetDefault<UPCGExGlobalEditorSettings>()->GetPropertyVisibility(FName("VariationScale")); }))
		.NameContent()
		[
			SNew(SVerticalBox)
			PCGEX_SMALL_LABEL_COL("Scale Min:Max", FLinearColor::White)
			+ SVerticalBox::Slot() // First line: toggle
			.AutoHeight()
			.Padding(0, 2, 0, 2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					PCGExEnumCustomization::CreateRadioGroup(ScaleSteppingModeHandle, FindFirstObjectSafe<UEnum>(*FString(TEXT("EPCGExStepping"))))
				]
				PCGEX_SMALL_LABEL("·· Uniform Scale : ")
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					UniformScaleHandle->CreatePropertyValueWidget()
				]
			]
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot() // Second line: X/Y/Z
			.Padding(0, 2, 0, 2)
			.AutoHeight()
			[
				// X pair
				SNew(SHorizontalBox)
				PCGEX_SMALL_LABEL(" X")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD(ScaleMinHandle, FVector, X, "Min X") PCGEX_SEP_LABEL(":") PCGEX_FIELD(ScaleMaxHandle, FVector, X, "Max X")
				]
				// Y pair
				PCGEX_SMALL_LABEL("·· Y")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD_BASE(ScaleMinHandle, FVector, Y, "Min Y") PCGEX_UNIFORM_VISIBILITY
					PCGEX_SEP_LABEL(":")
					PCGEX_FIELD_BASE(ScaleMaxHandle, FVector, Y, "Max Y") PCGEX_UNIFORM_VISIBILITY
				]
				// Z pair
				PCGEX_SMALL_LABEL("·· Z")
				+ SHorizontalBox::Slot().Padding(1).FillWidth(1)
				[
					SNew(SHorizontalBox) PCGEX_FIELD_BASE(ScaleMinHandle, FVector, Z, "Min Z") PCGEX_UNIFORM_VISIBILITY
					PCGEX_SEP_LABEL(":")
					PCGEX_FIELD_BASE(ScaleMaxHandle, FVector, Z, "Max Z") PCGEX_UNIFORM_VISIBILITY
				]
			]
			+ SVerticalBox::Slot() // Step
			.Padding(0, 0, 0, 2)
			.AutoHeight()
			[
				// X pair
				SNew(SHorizontalBox)
				PCGEX_STEP_VISIBILITY(ScaleSteppingModeHandle)
				PCGEX_SMALL_LABEL(" Steps : ")
				+ SHorizontalBox::Slot().FillWidth(1).Padding(1).VAlign(VAlign_Center)
				[
					PCGEX_VECTORINPUTBOX(ScaleStepsHandle)
				]
			]
		];

#undef PCGEX_UNIFORM_VISIBILITY

#pragma endregion

#undef PCGEX_SMALL_LABEL
#undef PCGEX_SEP_LABEL
#undef PCGEX_FIELD_BASE
#undef PCGEX_FIELD
#undef PCGEX_FIELD_D
#undef PCGEX_STEP_VISIBILITY
#undef PCGEX_VECTORINPUTBOX
#undef PCGEX_ROTATORINPUTBOX
}
