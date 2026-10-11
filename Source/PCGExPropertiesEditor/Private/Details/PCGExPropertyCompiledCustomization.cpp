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
#include "Widgets/SPCGExPropertyChoicePicker.h"
#include "Widgets/SPCGExPropertyChoicesEditor.h"
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
	// Resolve the outer property's USTRUCT name once; needed for inline-widget lookup
	// and for deciding which auxiliary meta fields (AllowedClass / Range) to show.
	const FStructProperty* OuterStructProp = CastField<FStructProperty>(PropertyHandle->GetProperty());
	FName OuterStructName = NAME_None;
	if (OuterStructProp && OuterStructProp->Struct)
	{
		OuterStructName = OuterStructProp->Struct->GetFName();
	}

	// The schema's own default follows the lock too. Single-object edits only: a multi-select has no one host.
	FStructView Host;
	{
		TArray<void*> RawData;
		PropertyHandle->AccessRawData(RawData);
		if (OuterStructProp && OuterStructProp->Struct && RawData.Num() == 1 && RawData[0])
		{
			Host = FStructView(OuterStructProp->Struct, static_cast<uint8*>(RawData[0]));
		}
	}
	bool bLocked = false;
	const bool bHasChoices = Host.IsValid() && FPCGExInlineWidgetRegistry::HasChoices(Host, &bLocked);

	// The arrow is the one entry point to choice authoring: its menu opens the popover over the real Choices handle.
	// Locked schemas (values-only hosts) get no entry, and no Choices row renders here: the popover owns it.
	FOnGetContent EditChoices;
	const TSharedPtr<IPropertyHandle> ChoicesHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExProperty, Choices));
	if (Host.IsValid() && ChoicesHandle.IsValid() && !FPCGExPropertyChoicesCustomization::IsUnderReadOnlySchema(PropertyHandle))
	{
		EditChoices = FOnGetContent::CreateLambda([Host, ChoicesHandle]() -> TSharedRef<SWidget>
		{
			FPCGExChoicesEditContext Context;
			Context.Host = Host;
			return SNew(SPCGExPropertyChoicesEditor).RealHandle(ChoicesHandle).Context(Context).bPatch(false);
		});
	}

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
		const FProperty* ChildProperty = ChildHandle->GetProperty();
		if (!ChildProperty || !ChildProperty->HasAnyPropertyFlags(CPF_Edit))
		{
			continue;
		}
		const FName ChildName = ChildProperty->GetFName();
		if (ChildName == GET_MEMBER_NAME_CHECKED(FPCGExProperty, Choices))
		{
			continue;
		}

		if (ChildName == TEXT("Value"))
		{
			const TSharedRef<IPropertyHandle> ValueHandle = ChildHandle.ToSharedRef();

			if (bHasChoices && bLocked)
			{
				FPCGExInlineWidgetRegistry::HookUnbindOnFreeEdit(ValueHandle, Host);
				ChildBuilder.AddProperty(ValueHandle).CustomWidget(/*bShowChildren=*/false)
				            .NameContent()
					[
						ValueHandle->CreatePropertyNameWidget()
					]
					.ValueContent()
					.MinDesiredWidth(250.0f)
					.MaxDesiredWidth(3000.0f)
					[
						SNew(SPCGExPropertyChoicePicker, Host, ValueHandle).Locked(true).OnGetEditContent(EditChoices)
					];
				continue;
			}

			if (const FPCGExMakeInlineWidgetFn* Factory = FPCGExInlineWidgetRegistry::Find(OuterStructName, EPCGExInlineWidgetMode::Edit))
			{
				TSharedRef<SWidget> ValueWidget = (*Factory)(ValueHandle);
				if (bHasChoices || EditChoices.IsBound())
				{
					ValueWidget = FPCGExInlineWidgetRegistry::WrapValueWidgetWithChoices(ValueWidget, Host, ValueHandle, EditChoices);
				}
				IDetailPropertyRow& Row = ChildBuilder.AddProperty(ValueHandle);
				Row.CustomWidget(/*bShowChildren=*/false)
				   .NameContent()
					[
						ValueHandle->CreatePropertyNameWidget()
					]
					.ValueContent()
					.MinDesiredWidth(250.0f)
					.MaxDesiredWidth(3000.0f)
					[
						ValueWidget
					];
				continue;
			}

			// Default editor (may expand); the quick pick / authoring entry gets its own row under it.
			ChildBuilder.AddProperty(ValueHandle);
			if (bHasChoices)
			{
				FPCGExInlineWidgetRegistry::HookUnbindOnFreeEdit(ValueHandle, Host);
			}
			if (bHasChoices || EditChoices.IsBound())
			{
				FPCGExInlineWidgetRegistry::AddChoicesRow(ChildBuilder, Host, ValueHandle, TAttribute<bool>(true), EditChoices);
			}
			continue;
		}

		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}
