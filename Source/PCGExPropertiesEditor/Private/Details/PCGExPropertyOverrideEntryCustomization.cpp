// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyOverrideEntryCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PCGExInlineWidgetRegistry.h"
#include "PCGExProperty.h"
#include "PCGExPropertyCollectionComponent.h"
#include "PCGExPropertySchema.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Details/PCGExEditorCustomizationUtils.h"
#include "Details/PCGExPropertyChoiceHost.h"
#include "Details/PCGExPropertyChoicesCustomization.h"
#include "Details/PCGExPropertyLabelRow.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "FPCGExPropertyOverrideEntryCustomization"

namespace PCGExPropertyOverrideEntryCustomization
{
	/** The row host these rows belong to: an outer implementing the interface, else the asset of the outer package. */
	UObject* FindRowHost(const TSharedRef<IPropertyHandle>& Handle)
	{
		TArray<UObject*> Outers;
		Handle->GetOuterObjects(Outers);
		for (UObject* Outer : Outers)
		{
			for (UObject* Candidate = Outer; Candidate; Candidate = Candidate->GetOuter())
			{
				if (Cast<IPCGExPropertyRowHost>(Candidate))
				{
					return Candidate;
				}
			}
		}

		// A struct-on-scope panel has no outer; the grid stamps the host's package on the scope.
		TArray<UPackage*> Packages;
		Handle->GetOuterPackages(Packages);
		for (const UPackage* Package : Packages)
		{
			UObject* Asset = Package ? Package->FindAssetInPackage() : nullptr;
			if (Asset && Cast<IPCGExPropertyRowHost>(Asset))
			{
				return Asset;
			}
		}
		return nullptr;
	}
}

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyOverrideEntryCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyOverrideEntryCustomization());
}

FPCGExPropertyOverrideEntry* FPCGExPropertyOverrideEntryCustomization::AccessEntry() const
{
	FPCGExPropertyOverrideEntry* First = nullptr;
	ForEachEntry([&First](FPCGExPropertyOverrideEntry& Entry)
	{
		if (!First)
		{
			First = &Entry;
		}
	});
	return First;
}

void FPCGExPropertyOverrideEntryCustomization::ForEachEntry(const TFunctionRef<void(FPCGExPropertyOverrideEntry&)> Visitor) const
{
	const TSharedPtr<IPropertyHandle> Handle = PropertyHandlePtr.Pin();
	if (!Handle.IsValid() || !Handle->IsValidHandle())
	{
		return;
	}
	Handle->EnumerateRawData([&Visitor](void* Data, const int32, const int32)
	{
		if (Data)
		{
			Visitor(*static_cast<FPCGExPropertyOverrideEntry*>(Data));
		}
		return true;
	});
}

const FPCGExProperty* FPCGExPropertyOverrideEntryCustomization::AccessEntryProperty() const
{
	const FPCGExPropertyOverrideEntry* Entry = AccessEntry();
	return Entry && Entry->Value.IsValid() ? Entry->Value.GetPtr<FPCGExProperty>() : nullptr;
}

FText FPCGExPropertyOverrideEntryCustomization::GetEntryNameText() const
{
	const FPCGExProperty* Prop = AccessEntryProperty();
	return Prop ? FText::FromName(Prop->PropertyName) : FText::FromString(TEXT("None"));
}

FText FPCGExPropertyOverrideEntryCustomization::GetEntryTypeText() const
{
	const FPCGExProperty* Prop = AccessEntryProperty();
	return Prop ? FText::FromName(Prop->GetDisplayTypeName()) : FText::FromString(TEXT("Unknown"));
}

FPCGExPropertyRowSite FPCGExPropertyOverrideEntryCustomization::BuildRowSite() const
{
	FPCGExPropertyRowSite Site;

	const TSharedPtr<IPropertyHandle> Handle = PropertyHandlePtr.Pin();
	const TSharedPtr<IPropertyHandle> ArrayHandle = Handle.IsValid() ? Handle->GetParentHandle() : nullptr;
	const TSharedPtr<IPropertyHandle> RowsHandle = ArrayHandle.IsValid() ? ArrayHandle->GetParentHandle() : nullptr;
	const FProperty* RowsProperty = RowsHandle.IsValid() ? RowsHandle->GetProperty() : nullptr;
	if (!RowsProperty)
	{
		return Site;
	}

	Site.RowsMember = RowsProperty->GetFName();
	Site.bIsImportLayer = Site.RowsMember == GET_MEMBER_NAME_CHECKED(FPCGExPropertySchemaCollection, ImportOverrides)
		&& RowsProperty->GetOwnerStruct() == FPCGExPropertySchemaCollection::StaticStruct();

	// Under an object view the rows member's parent is a struct property with raw data. Under a struct view (the grid's
	// entry or category-row copy) the parent is the structure node, which carries the copy instead.
	const TSharedPtr<IPropertyHandle> OuterHandle = RowsHandle->GetParentHandle();
	const FStructProperty* OuterProperty = OuterHandle.IsValid() ? CastField<FStructProperty>(OuterHandle->GetProperty()) : nullptr;
	if (OuterProperty)
	{
		if (const void* Raw = FPCGExPropertyHostRef::GetSingleRawData(OuterHandle))
		{
			Site.Outer = FConstStructView(OuterProperty->Struct, static_cast<const uint8*>(Raw));
		}
		return Site;
	}

	TArray<TSharedPtr<FStructOnScope>> Scopes;
	RowsHandle->GetOuterStructs(Scopes);
	if (Scopes.Num() == 1 && Scopes[0].IsValid())
	{
		if (const UScriptStruct* OuterStruct = Cast<UScriptStruct>(Scopes[0]->GetStruct()))
		{
			Site.Outer = FConstStructView(OuterStruct, Scopes[0]->GetStructMemory());
		}
	}
	return Site;
}

bool FPCGExPropertyOverrideEntryCustomization::ResolveInheritedValue(const FPCGExPropertyRowSite& Site, FInstancedStruct& Out) const
{
	const FPCGExPropertyOverrideEntry* Entry = AccessEntry();
	if (!Entry)
	{
		return false;
	}
	const FName PropertyName = Entry->GetPropertyName();

	if (const IPCGExPropertyRowHost* Host = Cast<IPCGExPropertyRowHost>(WeakRowHost.Get()))
	{
		return Host->EDITOR_ResolveInheritedValue(Site, PropertyName, Out);
	}

	// No host object: a struct-scope panel may have registered how the rows under its scope inherit.
	const TSharedPtr<IPropertyHandle> Handle = PropertyHandlePtr.Pin();
	TArray<TSharedPtr<FStructOnScope>> Scopes;
	if (Handle.IsValid())
	{
		Handle->GetOuterStructs(Scopes);
	}
	const PCGExPropertyRowHost::FResolveInheritedValue* Resolver = Scopes.Num() == 1
		? PCGExPropertyRowHost::FindScopeResolver(Scopes[0].Get())
		: nullptr;
	return Resolver && (*Resolver)(Site, PropertyName, Out);
}

bool FPCGExPropertyOverrideEntryCustomization::ResolveInheritedValue(FInstancedStruct& Out) const
{
	return ResolveInheritedValue(BuildRowSite(), Out);
}

bool FPCGExPropertyOverrideEntryCustomization::IsOverrideEnabled(const FPCGExPropertyOverrideEntry& Entry) const
{
	if (const UPCGExPropertyCollectionComponent* Live = WeakLiveComponent.Get())
	{
		const FName Name = Entry.GetPropertyName();
		return !Name.IsNone() && Live->EnabledOverrides.Contains(Name);
	}
	return Entry.bEnabled;
}

bool FPCGExPropertyOverrideEntryCustomization::IsOverrideEnabled() const
{
	const FPCGExPropertyOverrideEntry* Entry = AccessEntry();
	return Entry && IsOverrideEnabled(*Entry);
}

ECheckBoxState FPCGExPropertyOverrideEntryCustomization::GetToggleState() const
{
	// Mixed across a multi-object selection reads as undetermined, as the handle-bound checkbox did.
	TOptional<bool> Common;
	bool bMixed = false;
	ForEachEntry([this, &Common, &bMixed](const FPCGExPropertyOverrideEntry& Entry)
	{
		const bool bEnabled = IsOverrideEnabled(Entry);
		bMixed |= Common.IsSet() && *Common != bEnabled;
		Common = bEnabled;
	});
	if (!Common.IsSet() || bMixed)
	{
		return ECheckBoxState::Undetermined;
	}
	return *Common ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FPCGExPropertyOverrideEntryCustomization::OnToggleStateChanged(const ECheckBoxState NewState)
{
	SetOverrideEnabled(NewState == ECheckBoxState::Checked);
}

void FPCGExPropertyOverrideEntryCustomization::ApplyOverrideEnabled(const bool bEnable)
{
	// Resolved once, off the first entry: every entry the handle edits stands for the same property.
	FInstancedStruct Inherited;
	const bool bHasInherited = ResolveInheritedValue(Inherited);

	UPCGExPropertyCollectionComponent* Live = WeakLiveComponent.Get();
	ForEachEntry([bEnable, bHasInherited, &Inherited, Live](FPCGExPropertyOverrideEntry& Entry)
	{
		// A row of another type than its property is left to SyncToSchema: reassigning it here would free the memory
		// the live value rows alias before the rebuild lands.
		if (bHasInherited)
		{
			PCGExProperties::AdoptValue(Entry.Value, Inherited);
		}

		// The set first, so the mirror re-derived in the component's PostEditChangeChainProperty agrees. A None-named
		// row on an instance has no set entry to agree with: the mirror would revert at once, so it is left alone.
		const FName Name = Entry.GetPropertyName();
		if (Live)
		{
			if (Name.IsNone())
			{
				return;
			}
			Live->SetOverrideEnabled(Name, bEnable);
		}
		Entry.bEnabled = bEnable;
	});
}

void FPCGExPropertyOverrideEntryCustomization::SetOverrideEnabled(const bool bEnable)
{
	const TSharedPtr<IPropertyHandle> Handle = PropertyHandlePtr.Pin();
	if (!Handle.IsValid() || !AccessEntry())
	{
		return;
	}

	const FScopedTransaction Transaction(bEnable
		? LOCTEXT("EnableOverride", "Enable Property Override")
		: LOCTEXT("DisableOverride", "Disable Property Override"));

	// PreEditChange reaches every outer object (UObject::PreEditChange calls Modify); the grid's sync-back covers a copy.
	Handle->NotifyPreChange();
	ApplyOverrideEnabled(bEnable);
	Handle->NotifyPostChange(EPropertyChangeType::ValueSet);
	Handle->NotifyFinishedChangingProperties();

	RequestRowsRebuild();
}

void FPCGExPropertyOverrideEntryCustomization::RequestRowsRebuild() const
{
	// RequestRefresh refreshes the Slate tree over the same rows; RequestForceRefresh rebuilds them, next tick.
	if (const TSharedPtr<IPropertyUtilities> Utils = WeakPropertyUtilities.Pin())
	{
		Utils->RequestForceRefresh();
	}
}

TSharedRef<SWidget> FPCGExPropertyOverrideEntryCustomization::BuildOverrideToggleWidget()
{
	return SNew(SCheckBox)
		.IsChecked(this, &FPCGExPropertyOverrideEntryCustomization::GetToggleState)
		.OnCheckStateChanged(this, &FPCGExPropertyOverrideEntryCustomization::OnToggleStateChanged);
}

void FPCGExPropertyOverrideEntryCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Capture the entry + its Value handle as members. Every lambda / widget created below (and in CustomizeChildren)
	// reads through these members, so their backing nodes stay reachable for the full lifetime of this customization.
	PropertyHandlePtr = PropertyHandle;
	ValueHandlePtr = PropertyHandle->GetChildHandle(TEXT("Value"));
	WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();

	WeakRowHost = PCGExPropertyOverrideEntryCustomization::FindRowHost(PropertyHandle);
	WeakLiveComponent.Reset();
	WeakOwner.Reset();
	bComponentHasArchetypeChain = false;

	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		if (!Outer || Outer->IsTemplate())
		{
			continue;
		}

		if (!WeakOwner.IsValid())
		{
			WeakOwner = Outer;
		}

		if (UPCGExPropertyCollectionComponent* Comp = Cast<UPCGExPropertyCollectionComponent>(Outer))
		{
			WeakLiveComponent = Comp;
			bComponentHasArchetypeChain = (Comp->CreationMethod != EComponentCreationMethod::Instance);
			break;
		}
	}

	// Check if this is an inline type (schema-driven, stable for the detail session)
	bool bShouldInline = false;
	if (ValueHandlePtr.IsValid())
	{
		TArray<void*> RawData;
		ValueHandlePtr->AccessRawData(RawData);
		if (!RawData.IsEmpty() && RawData[0])
		{
			FInstancedStruct* Instance = static_cast<FInstancedStruct*>(RawData[0]);
			if (Instance && Instance->IsValid())
			{
				UScriptStruct* InnerStruct = const_cast<UScriptStruct*>(Instance->GetScriptStruct());
				if (InnerStruct)
				{
					bShouldInline = FPCGExInlineWidgetRegistry::IsInlineValue(InnerStruct, EPCGExInlineWidgetMode::Compact);
				}
			}
		}
	}

	// For complex (non-inline) types, show checkbox + label in header
	// For simple (inline) types, header stays empty (everything in CustomizeChildren)
	if (!bShouldInline)
	{
		HeaderRow
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 4, 0)
				[
					BuildOverrideToggleWidget()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					PCGExPropertyLabelRow::Build(
						TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::GetEntryNameText)),
						TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::GetEntryTypeText)))
				]
			];

		// Complex types put their content on HeaderRow; the reset arrow lives on this row. Inline types leave HeaderRow
		// empty and their inner Value row inherits NoResetToDefault from the FPCGExProperty subclass's Value UPROPERTY.
		HeaderRow.OverrideResetToDefault(MakeArchetypeResetOverride());
	}
}

FResetToDefaultOverride FPCGExPropertyOverrideEntryCustomization::MakeArchetypeResetOverride()
{
	return FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::IsResetToInheritedVisible),
		FResetToDefaultHandler::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::ResetToInherited),
		/*bPropagateToChildren*/ true);
}

bool FPCGExPropertyOverrideEntryCustomization::IsResetToInheritedVisible(TSharedPtr<IPropertyHandle>) const
{
	// A disabled row already shows the chain's value: only an authored (enabled) slot has anything to defer.
	return bComponentHasArchetypeChain && IsOverrideEnabled();
}

void FPCGExPropertyOverrideEntryCustomization::ResetToInherited(TSharedPtr<IPropertyHandle>)
{
	// The engine runs this inside its own transaction and pre/post notify bracket (FPropertyHandleBase::
	// OnCustomResetToDefault): a second NotifyPreChange would hit UActorComponent::PreEditChange's re-entry check.
	if (bComponentHasArchetypeChain)
	{
		ApplyOverrideEnabled(false);
		RequestRowsRebuild();
	}
}

void FPCGExPropertyOverrideEntryCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// CustomizeHeader is always called first and populates the member handles,
	// but recover gracefully if we are somehow entered cold.
	if (!ValueHandlePtr.IsValid())
	{
		ValueHandlePtr = PropertyHandle->GetChildHandle(TEXT("Value"));
	}

	if (!ValueHandlePtr.IsValid())
	{
		return;
	}

	// Access raw data to get the FInstancedStruct
	TArray<void*> RawData;
	ValueHandlePtr->AccessRawData(RawData);
	if (RawData.IsEmpty() || !RawData[0])
	{
		return;
	}

	FInstancedStruct* Instance = static_cast<FInstancedStruct*>(RawData[0]);
	if (!Instance || !Instance->IsValid())
	{
		return;
	}

	UScriptStruct* InnerStruct = const_cast<UScriptStruct*>(Instance->GetScriptStruct());
	if (!InnerStruct)
	{
		return;
	}

	const FPCGExPropertyRowSite Site = BuildRowSite();

	// A disabled slot shows what its consumers resolve to, off a copy this customization owns: its stored value is
	// never shown. The slot itself is shown while enabled, or when nothing below declares the property in its type.
	uint8* StructMemory = Instance->GetMutableMemory();
	InheritedValue.Reset();
	if (!IsOverrideEnabled() && ResolveInheritedValue(Site, InheritedValue) && InheritedValue.GetScriptStruct() == InnerStruct)
	{
		StructMemory = InheritedValue.GetMutableMemory();
	}
	else
	{
		InheritedValue.Reset();
	}
	if (!StructMemory)
	{
		return;
	}
	const bool bShowsInherited = InheritedValue.IsValid();

	const bool bShouldInline = FPCGExInlineWidgetRegistry::IsInlineValue(InnerStruct, EPCGExInlineWidgetMode::Compact);

	// Non-owning: StructMemory is the slot the panel keeps alive for the session, or InheritedValue above, a member.
	InnerScope = MakeShared<FStructOnScope>(InnerStruct, StructMemory);

	// The row's choices are read off what the row shows: the slot through the entry's own Value handle, which follows
	// it across a reallocation; the inherited copy through its scope, which this instance owns.
	FPCGExChoiceRowArgs Choices;
	Choices.Host = bShowsInherited
		? FPCGExPropertyHostRef(InnerScope)
		: FPCGExPropertyHostRef(ValueHandlePtr, EPCGExPropertyHostKind::Instanced);

	// An import entry authors this importer's rewrite of the inherited choices through the arrow's popover. Not under
	// a locked schema (values-only host), and not on a Blueprint-built component instance, which keeps no patch of
	// its own: its class chain's applies. Consumer rows (a Tuple's, an entry's) get no entry: the arrow stays a pick.
	// The patch session needs the entry's own Value as its host (it checks the memory matches), never the display copy.
	const bool bKeepsPatch = !(WeakLiveComponent.IsValid() && bComponentHasArchetypeChain);
	if (bKeepsPatch
		&& Site.bIsImportLayer
		&& !PCGExPropertyChoicesUI::IsUnderReadOnlySchema(PropertyHandle))
	{
		Choices.EditChoices = PCGExPropertyChoicesUI::MakePatchEditor(PropertyHandle, FPCGExPropertyHostRef(ValueHandlePtr, EPCGExPropertyHostKind::Instanced));
	}

	const TAttribute<bool> IsEnabledAttr = TAttribute<bool>::Create(
		TAttribute<bool>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::IsOverrideEnabled));

	if (bShouldInline)
	{
		const TSharedRef<SWidget> NameContent = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				BuildOverrideToggleWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PCGExPropertyLabelRow::Build(
					TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::GetEntryNameText)),
					TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPCGExPropertyOverrideEntryCustomization::GetEntryTypeText)))
			];

		IDetailPropertyRow* InnerRow = FPCGExInlineWidgetRegistry::AddCompactValueRow(
			ChildBuilder, InnerScope.ToSharedRef(), InnerStruct, NameContent, IsEnabledAttr, Choices);
		if (InnerRow)
		{
			// Reset arrow defers to the BP class chain -- only for BP-inherited owners. Instance-created
			// components have none; their inner Value row keeps its NoResetToDefault meta.
			if (bComponentHasArchetypeChain)
			{
				InnerRow->OverrideResetToDefault(MakeArchetypeResetOverride());
			}
			if (WeakOwner.IsValid())
			{
				PCGExEditorCustomizationUtils::HookOwnerChangeOnHandleChanged(InnerRow->GetPropertyHandle(), WeakOwner);
			}
		}
	}
	else
	{
		FPCGExInlineWidgetRegistry::AddComplexValueRows(
			ChildBuilder, InnerScope.ToSharedRef(), InnerStruct, IsEnabledAttr, WeakOwner, Choices);
	}
}

#undef LOCTEXT_NAMESPACE
