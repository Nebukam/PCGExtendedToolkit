// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExFittingVariationsCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Collections/PCGExAssetCollection.h"
#include "Constants/PCGExTuple.h"
#include "Math/UnitConversion.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExFittingVariationsCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExFittingVariationsCustomization());
}

void FPCGExFittingVariationsCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
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
.ToolTipText(FString(_TOOLTIP).IsEmpty() ? _HANDLE->GetToolTipText() : FText::FromString(_TOOLTIP))

#define PCGEX_FIELD(_HANDLE, _TYPE, _PART, _TOOLTIP) PCGEX_FIELD_BASE(_HANDLE, _TYPE, _PART, _TOOLTIP)]

#pragma region Offset Min/Max

	// Get handles
	TSharedPtr<IPropertyHandle> OffsetMinHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, OffsetMin));
	TSharedPtr<IPropertyHandle> OffsetMaxHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, OffsetMax));
	TSharedPtr<IPropertyHandle> AbsoluteOffsetHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, bAbsoluteOffset));

	// Add custom Offset row
	ChildBuilder.AddCustomRow(FText::FromString("Offset"))
	            .NameContent()
		[
			SNew(SVerticalBox)
			PCGEX_SMALL_LABEL_COL("Offset Min:Max", FLinearColor::White)
			+ SVerticalBox::Slot() // First line: toggle
			.AutoHeight()
			.Padding(0,2,0,8)
			[
				SNew(SHorizontalBox)
				PCGEX_SMALL_LABEL("Absolute Space : ")
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
			+ SVerticalBox::Slot() // Second line: X/Y/Z
			.Padding(0, 2, 0, 8)
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
		];

#pragma endregion

#pragma region Rotation Min/Max


	// Get handles
	TSharedPtr<IPropertyHandle> RotationMinHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, RotationMin));
	TSharedPtr<IPropertyHandle> RotationMaxHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, RotationMax));
	TSharedPtr<IPropertyHandle> AbsoluteRotHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, AbsoluteRotation));

	// Add custom Offset row
	ChildBuilder.AddCustomRow(FText::FromString("Rotation"))
	            .NameContent()
		[
			SNew(SVerticalBox)
			PCGEX_SMALL_LABEL_COL("Rotation Min:Max", FLinearColor::White)
			+ SVerticalBox::Slot() // First line: toggle
			.AutoHeight()
			.Padding(0,2,0,8)
			[
				SNew(SHorizontalBox)
				PCGEX_SMALL_LABEL("Absolute Rotation : ")
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2, 0)
				[
					AbsoluteRotHandle->CreatePropertyValueWidget()
				]
			]
		]
		.ValueContent()
		.MinDesiredWidth(400)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot() // Second line: X/Y/Z
			.Padding(0, 2, 0, 8)
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
		];

#pragma endregion

#pragma region Rotation Min/Max

	// Get handles
	TSharedPtr<IPropertyHandle> ScaleMinHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, ScaleMin));
	TSharedPtr<IPropertyHandle> ScaleMaxHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, ScaleMax));
	TSharedPtr<IPropertyHandle> UniformScaleHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExFittingVariations, bUniformScale));

#define PCGEX_UNIFORM_VISIBILITY .IsEnabled_Lambda([UniformScaleHandle]() { bool bUniform = false; UniformScaleHandle->GetValue(bUniform); return !bUniform; }) ]

	// Add custom Offset row
	ChildBuilder.AddCustomRow(FText::FromString("Scale"))
	            .NameContent()
		[
			SNew(SVerticalBox)
			PCGEX_SMALL_LABEL_COL("Scale Min:Max", FLinearColor::White)
			+ SVerticalBox::Slot() // First line: toggle
			.AutoHeight()
			.Padding(0,2,0,8)
			[
				SNew(SHorizontalBox)
				PCGEX_SMALL_LABEL("Uniform Scale : ")
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
			.Padding(0, 2, 0, 8)
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
		];

#undef PCGEX_UNIFORM_VISIBILITY

#pragma endregion

#undef PCGEX_SMALL_LABEL
#undef PCGEX_SEP_LABEL
#undef PCGEX_FIELD_BASE
#undef PCGEX_FIELD
#undef PCGEX_FIELD_D
}
