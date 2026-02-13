// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FPCGExValencyCageEditorMode;

/**
 * Compact horizontal row of toggle buttons for controlling viewport visualization layers.
 * Each toggle controls a boolean in FValencyVisibilityFlags.
 */
class SValencyVisToggles : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValencyVisToggles) {}
		SLATE_ARGUMENT(FPCGExValencyCageEditorMode*, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FPCGExValencyCageEditorMode* EditorMode = nullptr;

	/** Create a single toggle button */
	TSharedRef<SWidget> MakeToggleButton(const FText& Label, const FText& Tooltip, bool* FlagPtr);

	/** Force viewport redraw after toggle */
	void RedrawViewports() const;
};
