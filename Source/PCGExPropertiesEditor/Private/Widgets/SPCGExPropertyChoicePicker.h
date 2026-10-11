// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Widgets/SCompoundWidget.h"

class FPCGExChoiceRowState;
class IPropertyHandle;
class SComboButton;

/**
 * Choice dropdown for a property row, driven by the row's shared state: it holds no pointer into the edited data.
 * Locked: a full button (current choice or "Custom", plus the value preview) standing in for the value editor;
 * unlocked: a bare arrow beside it. A pick is one FPCGExRawPropertyEdit commit on NotifyHandle, like a typed edit.
 */
class SPCGExPropertyChoicePicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExPropertyChoicePicker)
			: _Locked(false)
			, _CanPick(true)
		{
		}

		/** The full button (current choice + value preview) instead of the bare arrow. */
		SLATE_ARGUMENT(bool, Locked)

		/** Gates the choice entries only: "Edit Choices..." stays reachable on a row whose value override is off. */
		SLATE_ATTRIBUTE(bool, CanPick)

		/** Bound: the menu leads with "Edit Choices...", which opens this content as a popover under the button. */
		SLATE_EVENT(FOnGetContent, OnGetEditContent)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FPCGExChoiceRowState>& InState, const TSharedRef<IPropertyHandle>& InNotifyHandle);

private:
	FText GetSelectionLabel() const;
	FText GetSelectionPreview() const;
	FSlateColor GetSelectionColor() const;
	/** The arrow: accent blue while bound, the style's foreground otherwise. */
	FSlateColor GetArrowColor() const;
	FText GetSelectionToolTip() const;
	TSharedRef<SWidget> BuildMenu();
	void PickChoice(FGuid ChoiceId);
	/** Closes the menu, then pushes the edit content as a popover anchored under the button. */
	void OpenEditor();

	TSharedPtr<FPCGExChoiceRowState> State;
	TSharedPtr<IPropertyHandle> NotifyHandle;
	bool bLocked = false;
	TAttribute<bool> CanPick;
	FOnGetContent OnGetEditContent;
	TSharedPtr<SComboButton> ComboButton;
};
