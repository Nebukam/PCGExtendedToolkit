// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "AssemblyRoot/PCGExAssemblyRootEditorActions.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Viewport overlay bar for the stock assembly root's quick actions. Hosted on the active level viewport
 * by FPCGExAssemblyRootEditorHost only while a stock root is in play; rows disable rather than hide so
 * the bar always advertises what it can do.
 */
class SPCGExAssemblyRootBar final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExAssemblyRootBar)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static constexpr float ButtonHeight = 28.0f;

private:
	void BuildButtonStyle();
	TSharedRef<SWidget> MakeActionButton(PCGExAssemblyRootEditor::EAction Action);
	/** Per-frame memo: every button's enabled binding and tint poll each Slate frame, and the answer walks the selection. */
	bool CanExecute(PCGExAssemblyRootEditor::EAction Action) const;

	FButtonStyle ButtonStyle;

	mutable uint64 MemoFrame = MAX_uint64;
	mutable bool bMemoCanExecute[3] = {false, false, false};
};
