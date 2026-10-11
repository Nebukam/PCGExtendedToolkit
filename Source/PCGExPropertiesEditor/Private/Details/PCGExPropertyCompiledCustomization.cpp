// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyCompiledCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "PCGExInlineWidgetRegistry.h"
#include "PCGExProperty.h"
#include "PropertyHandle.h"
#include "Details/PCGExPropertyChoicesCustomization.h"

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyCompiledCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyCompiledCustomization());
}

void FPCGExPropertyCompiledCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Collapse the inner group header that FInstancedStructDetails would otherwise inject
	// between the type-picker row and the Value field. The schema header already shows
	// the property name and type, so this wrapper is pure noise.
	HeaderRow.Visibility(EVisibility::Collapsed);
}

void FPCGExPropertyCompiledCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Resolve the outer property's USTRUCT once; needed for inline-widget lookup and for how its Value row is laid out.
	const FStructProperty* OuterStructProp = CastField<FStructProperty>(PropertyHandle->GetProperty());
	const UScriptStruct* OuterStruct = OuterStructProp ? OuterStructProp->Struct.Get() : nullptr;
	const FName OuterStructName = OuterStruct ? OuterStruct->GetFName() : NAME_None;
	const FProperty* ValueProperty = PCGExProperties::FindValueProperty(OuterStruct);

	// The schema's own default follows the lock too. The arrow is the one entry point to choice authoring: its menu
	// opens the popover over the real Choices handle. Locked schemas (values-only hosts) get no entry.
	FPCGExChoiceRowArgs Choices;
	Choices.Host = FPCGExPropertyHostRef(PropertyHandle, EPCGExPropertyHostKind::Property);
	if (!PCGExPropertyChoicesUI::IsUnderReadOnlySchema(PropertyHandle))
	{
		Choices.EditChoices = PCGExPropertyChoicesUI::MakeDeclarationEditor(
			PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExProperty, Choices)), Choices.Host);
	}
	// A value editor that also sets the type's structure (an enum's class, a struct's type, an entry's collection)
	// cannot give way to the picker: nothing else on this row edits that structure.
	Choices.bKeepEditorWhenLocked = OuterStruct && OuterStruct->HasMetaData(TEXT("PCGExStructuralValue"));

	// Iterate all children and render in declaration order, skipping internal fields.
	// "Value" gets the registered Edit-mode inline widget when available; all other
	// authored fields (e.g. editor-only AllowedClass / Range on opted-in types) fall
	// through to default rendering, which lets their own IPropertyTypeCustomization
	// (if any) take over.
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; ++i)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(i);
		if (!ChildHandle.IsValid())
		{
			continue;
		}

		// Identity caches and the recorded pick are not editable; nothing to render for them.
		// Choices has no row of its own: the popover is its editor.
		const FProperty* ChildProperty = ChildHandle->GetProperty();
		if (!ChildProperty || !ChildProperty->HasAnyPropertyFlags(CPF_Edit)
			|| ChildProperty->GetFName() == GET_MEMBER_NAME_CHECKED(FPCGExProperty, Choices))
		{
			continue;
		}

		if (ChildProperty != ValueProperty)
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
			continue;
		}

		const TSharedRef<IPropertyHandle> ValueHandle = ChildHandle.ToSharedRef();
		const TSharedRef<FPCGExChoiceRowState> State = MakeShared<FPCGExChoiceRowState>(Choices.Host);
		const FPCGExMakeInlineWidgetFn* Factory = FPCGExInlineWidgetRegistry::Find(OuterStructName, EPCGExInlineWidgetMode::Edit);

		// One row: the editor, with the arrow in the same cell. The same layout a consumer row of this type gets.
		if (Factory || FPCGExInlineWidgetRegistry::IsInlineValue(OuterStruct, EPCGExInlineWidgetMode::Edit))
		{
			// Widths are fixed when the row is built, like AddCompactValueRow's.
			const bool bWide = Factory != nullptr || State->Get().bLocked;
			ChildBuilder.AddProperty(ValueHandle).CustomWidget(/*bShowChildren=*/false)
			            .NameContent()
				[
					ValueHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MinDesiredWidth(bWide ? FPCGExInlineWidgetRegistry::WideValueMinWidth : FPCGExInlineWidgetRegistry::NarrowValueMinWidth)
				.MaxDesiredWidth(bWide ? FPCGExInlineWidgetRegistry::WideValueMaxWidth : FPCGExInlineWidgetRegistry::NarrowValueMaxWidth)
				[
					FPCGExInlineWidgetRegistry::WrapValueWidgetWithChoices(
						Factory ? (*Factory)(ValueHandle) : ValueHandle->CreatePropertyValueWidget(),
						State, ValueHandle, TAttribute<bool>(true), Choices)
				];
			continue;
		}

		// An editor that may expand: the choices get their own row under it, which stands in for it while locked.
		IDetailPropertyRow& ValueRow = ChildBuilder.AddProperty(ValueHandle);
		if (!Choices.bKeepEditorWhenLocked)
		{
			ValueRow.Visibility(FPCGExInlineWidgetRegistry::VisibleUnlessLocked(State));
		}
		FPCGExInlineWidgetRegistry::AddChoicesRow(ChildBuilder, State, ValueHandle, TAttribute<bool>(true), Choices.EditChoices);
	}
}
