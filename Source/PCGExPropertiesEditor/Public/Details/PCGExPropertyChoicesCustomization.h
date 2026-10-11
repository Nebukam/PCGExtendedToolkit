// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"
#include "Input/Reply.h"
#include "StructUtils/StructView.h"

class FInstancedStructProvider;
class IPropertyUtilities;
struct FPCGExPropertyChoices;
struct FPCGExPropertyChoicesPatch;
struct FPCGExPropertySchemaCollection;
struct FPropertyChangedEvent;

/**
 * What the choices customizations cannot walk to from a scratch copy: the property the choices belong to and, for a
 * patch, where it sits. The popover editor registers one per scratch struct for its lifetime.
 */
struct FPCGExChoicesEditContext
{
	/** The property instance the choices answer to (an import row's Value for a patch). */
	FStructView Host;
	/** Patch only: the collection whose import rows the patch lives in. */
	FPCGExPropertySchemaCollection* Collection = nullptr;
	/** Patch only: the import entry's property name. */
	FName EntryName = NAME_None;
	/** Patch only: the real patch, the one the resolve chain points at (the scratch copy is not). */
	const FPCGExPropertyChoicesPatch* RealPatch = nullptr;
};

/** RAII registration of a FPCGExChoicesEditContext under the address of the struct being edited. */
class PCGEXPROPERTIESEDITOR_API FPCGExChoicesEditScope
{
public:
	FPCGExChoicesEditScope(const void* InKey, const FPCGExChoicesEditContext& InContext);
	~FPCGExChoicesEditScope();

	static const FPCGExChoicesEditContext* Find(const void* Key);

private:
	const void* Key = nullptr;
};

/**
 * FPCGExPropertyChoices in schema-edit mode. Header strip: [Locked] [N choices] [Add Current Value]; children: one
 * row per choice (FPCGExPropertyChoiceCustomization), with the engine's element buttons. Collapsed entirely under a
 * ReadOnlySchema ancestor: values-only hosts never author choices.
 */
class FPCGExPropertyChoicesCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** True when an ancestor handle carries ReadOnlySchema (values-only hosts): no choice authoring of any kind. */
	static bool IsUnderReadOnlySchema(const TSharedRef<IPropertyHandle>& Handle);

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FText GetCountText() const;
	FReply OnAddCurrentValue();
	void OnNumItemsChanged();

	TWeakPtr<IPropertyHandle> ChoicesHandlePtr;
	TSharedPtr<IPropertyHandle> ItemsHandle;
	TWeakPtr<IPropertyUtilities> WeakPropertyUtilities;
	bool bReadOnly = false;
};

/**
 * One choice as one row: the Label editor as the name, the carrier's own Value editor as the value (Compact factory,
 * else the default widget). A carrier of a complex type keeps a preview in the header and expands to its editor.
 * A carrier the host type no longer accepts shows why instead of an editor.
 */
class FPCGExPropertyChoiceCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	/** FPCGExPropertyChoice::Value, the carrier FInstancedStruct. */
	TSharedPtr<IPropertyHandle> CarrierHandle;
	/** Keeps the carrier's child-structure nodes alive for the row's lifetime. */
	TSharedPtr<FInstancedStructProvider> CarrierProvider;
	/** The carrier's own "Value" field. */
	TSharedPtr<IPropertyHandle> CarrierValueHandle;
	bool bInlineCarrier = false;
};

/**
 * FPCGExPropertyChoicesPatch on an import override row: this importer's rewrite of the inherited choices. Header:
 * [Mode] [Lock] [N effective] [Add Current Value]. Children: the inherited choices (read-only, with Override / Remove /
 * Restore; hidden under Replace) followed by this patch's own items (hidden under Inherit). Any action on an inherited
 * choice flips Inherit to Modify. Every committed edit re-mirrors the collection's import rows in place.
 */
class FPCGExPropertyChoicesPatchCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	FPCGExPropertyChoicesPatch* GetPatch() const;
	/** The import row's own property (its Value), the host every carrier of this patch answers to. */
	FStructView GetHost() const;
	/** What this importer inherits: the effective list with this patch left out. */
	void GetInheritedChoices(FPCGExPropertyChoices& Out) const;
	FText GetCountText() const;

	FReply OnAddCurrentValue();
	FReply OnOverrideInherited(FGuid ChoiceId);
	FReply OnRemoveInherited(FGuid ChoiceId);
	FReply OnRestoreInherited(FGuid ChoiceId);
	/** Inherit never applies a rewrite: the first action on the list switches to Modify. */
	void EnsureModifyMode();

	void OnPatchChanged(const FPropertyChangedEvent& InEvent);
	void SyncImportRowsInPlace();
	void RequestRebuild();

	TWeakPtr<IPropertyHandle> PatchHandlePtr;
	TSharedPtr<IPropertyHandle> ModeHandle;
	TSharedPtr<IPropertyHandle> LockHandle;
	TSharedPtr<IPropertyHandle> ItemsHandle;
	TSharedPtr<IPropertyHandle> RemovedIdsHandle;
	TWeakPtr<IPropertyUtilities> WeakPropertyUtilities;
	bool bReadOnly = false;
};
