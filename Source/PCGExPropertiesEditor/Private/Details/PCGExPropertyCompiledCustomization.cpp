// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyCompiledCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "PCGExPropertyCompiled.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyCompiledCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyCompiledCustomization());
}

void FPCGExPropertyCompiledCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Get the property name to display as header
	const FName PropName = GetPropertyName(PropertyHandle);
	const FString DisplayText = PropName.IsNone() ? TEXT("(Unnamed)") : PropName.ToString();

	// Get type name for context
	FString TypeName = TEXT("Property");
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	if (!RawData.IsEmpty() && RawData[0])
	{
		if (const FPCGExPropertyCompiled* Prop = static_cast<const FPCGExPropertyCompiled*>(RawData[0]))
		{
			TypeName = Prop->GetTypeName().ToString();
		}
	}

	HeaderRow.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(DisplayText))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8, 0, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("(%s)"), *TypeName)))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(FSlateColor(FLinearColor::Gray * 0.6f))
		]
	];
}

void FPCGExPropertyCompiledCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Try to get Value handle directly first
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));
	if (ValueHandle.IsValid())
	{
		ChildBuilder.AddProperty(ValueHandle.ToSharedRef());
		return;
	}

	// Fallback: Show all children except PropertyName
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(i);
		if (!ChildHandle.IsValid()) { continue; }

		const FName ChildName = ChildHandle->GetProperty() ? ChildHandle->GetProperty()->GetFName() : NAME_None;

		// Skip PropertyName - it's shown in the header
		if (ChildName == TEXT("PropertyName")) { continue; }

		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}

FName FPCGExPropertyCompiledCustomization::GetPropertyName(TSharedRef<IPropertyHandle> PropertyHandle) const
{
	TSharedPtr<IPropertyHandle> NameHandle = PropertyHandle->GetChildHandle(TEXT("PropertyName"));
	if (NameHandle.IsValid())
	{
		FName Value;
		NameHandle->GetValue(Value);
		return Value;
	}
	return NAME_None;
}
