// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExPropertyChoiceHost.h"
#include "Framework/SlateDelegates.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IDetailPropertyRow;
class IDetailChildrenBuilder;
class FStructOnScope;
class IPropertyHandle;
class SWidget;
class UScriptStruct;

/**
 * Factory signature for an inline widget.
 *
 * ValueHandle is the IPropertyHandle of the FInstancedStruct's inner struct's "Value" field
 * (equivalent of what AddProperty(ValueHandle) would render in the default inline path).
 * The factory is expected to return a single-row compact widget that fits into the
 * override entry's ValueContent slot (no expandable children).
 */
using FPCGExMakeInlineWidgetFn = TFunction<TSharedRef<SWidget>(const TSharedRef<IPropertyHandle>& /*ValueHandle*/)>;

/**
 * Distinguishes the two contexts in which inline widgets are rendered.
 *
 * Some property types (e.g. enum) need different UI affordances depending on whether the
 * user is *defining* the property (Edit) or *overriding/displaying* a previously-defined
 * one (Compact). For example, an enum schema editor needs to expose the enum class picker;
 * an enum override only needs to pick a value within the already-pinned class.
 *
 * For property types where the same widget works in both contexts (Vector, Vector2,
 * Rotator), register the same factory under both modes.
 */
enum class EPCGExInlineWidgetMode : uint8
{
	/** Schema-edit context: full editor including any "definition" controls (e.g. enum class picker). */
	Edit,
	/** Override / readonly-schema context: compact value-only editor. */
	Compact,
};

/**
 * Registry of custom inline widget factories keyed by the FName of the outer
 * FPCGExProperty subclass UScriptStruct (e.g. "PCGExProperty_Vector") and a render mode.
 *
 * When a factory is found, it is used in place of the default AddProperty(ValueHandle)
 * behavior; otherwise the existing default is preserved.
 *
 * External editor modules can register their own factories during StartupModule().
 */
class PCGEXPROPERTIESEDITOR_API FPCGExInlineWidgetRegistry
{
public:
	/** Register a factory under a single mode. */
	static void Register(FName StructName, EPCGExInlineWidgetMode Mode, FPCGExMakeInlineWidgetFn Factory);

	/** Convenience: register the same factory under both Edit and Compact modes. */
	static void RegisterAllModes(FName StructName, FPCGExMakeInlineWidgetFn Factory);

	static void Unregister(FName StructName, EPCGExInlineWidgetMode Mode);
	static void UnregisterAllModes(FName StructName);

	static const FPCGExMakeInlineWidgetFn* Find(FName StructName, EPCGExInlineWidgetMode Mode);

	static void Clear();

	/**
	 * Adds a flat CustomWidget row for a PCGExInlineValue struct's Value field using the Compact registry.
	 * Falls back to IPropertyHandle::CreatePropertyValueWidget() when no factory is registered.
	 * NameContent is caller-supplied -- pass a plain STextBlock for locked schemas, an SHorizontalBox
	 * with a checkbox for override entries, etc.
	 * Scope must outlive the detail panel session; caller is responsible for keeping it alive.
	 * Returns the IDetailPropertyRow when a "Value" FProperty was found and the row was added; nullptr
	 * otherwise. Callers that need to attach a reset-to-default override (or any other row-level
	 * customization) use the returned row pointer.
	 */
	static IDetailPropertyRow* AddCompactValueRow(
		IDetailChildrenBuilder& ChildBuilder,
		TSharedRef<FStructOnScope> Scope,
		UScriptStruct* InnerStruct,
		TSharedRef<SWidget> NameContent,
		TAttribute<bool> IsEnabled = TAttribute<bool>(true),
		const FPCGExChoiceRowArgs& Choices = FPCGExChoiceRowArgs());

	/**
	 * Adds every editable property of InnerStruct as a child row, minus the internal OutputBuffer and Choices
	 * fields. Applies IsEnabled to each row.
	 * Used for complex (non-PCGExInlineValue) property types shown in override or read-only-schema contexts.
	 *
	 * When a Compact factory is registered for InnerStruct (and the type is NOT PCGExInlineValue,
	 * or it wouldn't reach this path), the "Value" field renders through the factory as a
	 * full-width (WholeRowContent) row instead of the default property row -- for editors whose
	 * width must be pinned by the panel (e.g. the Float Curve editor).
	 *
	 * Pass WeakOwner to hook each emitted row's value-change through
	 * PCGExEditorCustomizationUtils::HookOwnerChangeOnHandleChanged -- required when the owner
	 * needs to dirty + signal PCG re-cook for these external-struct edits.
	 */
	static void AddComplexValueRows(
		IDetailChildrenBuilder& ChildBuilder,
		TSharedRef<FStructOnScope> Scope,
		UScriptStruct* InnerStruct,
		TAttribute<bool> IsEnabled = TAttribute<bool>(true),
		const TWeakObjectPtr<UObject>& WeakOwner = nullptr,
		const FPCGExChoiceRowArgs& Choices = FPCGExChoiceRowArgs());

	// --- Choices ---
	// Every affordance reads the row's FPCGExChoiceRowState, so it follows the lock and the list as they change.
	// IsEnabled gates the value editor and the choice entries, never the arrow: "Edit Choices..." has to stay
	// reachable on an import row whose value override is off.

	/**
	 * The value cell of a one-row editor: the editor with its bound-choice badge, the picker that stands in for it
	 * while locked, and the quick-pick arrow while unlocked.
	 */
	static TSharedRef<SWidget> WrapValueWidgetWithChoices(
		TSharedRef<SWidget> ValueWidget,
		const TSharedRef<FPCGExChoiceRowState>& State,
		const TSharedRef<IPropertyHandle>& NotifyHandle,
		TAttribute<bool> IsEnabled,
		const FPCGExChoiceRowArgs& Choices);

	/**
	 * A standalone "Choices" row for layouts whose value spans several rows: the current choice while locked, a
	 * quick-pick arrow otherwise. Collapsed while there is nothing to pick and nothing to author.
	 */
	static void AddChoicesRow(
		IDetailChildrenBuilder& ChildBuilder,
		const TSharedRef<FPCGExChoiceRowState>& State,
		const TSharedRef<IPropertyHandle>& NotifyHandle,
		TAttribute<bool> IsEnabled,
		FOnGetContent EditChoices);

	/** Visibility for a value row the Choices row stands in for: collapsed while State is locked. */
	static TAttribute<EVisibility> VisibleUnlessLocked(const TSharedRef<FPCGExChoiceRowState>& State);

	/** Value cell widths: wide for a row showing a factory widget or the locked picker, narrow for a default editor. */
	static constexpr float WideValueMinWidth = 250.0f;
	static constexpr float WideValueMaxWidth = 3000.0f;
	static constexpr float NarrowValueMinWidth = 125.0f;
	static constexpr float NarrowValueMaxWidth = 600.0f;
};
