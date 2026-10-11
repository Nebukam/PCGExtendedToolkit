// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Details/PCGExPropertyChoicesCustomization.h"
#include "Widgets/SCompoundWidget.h"

class IPropertyHandle;
class IStructureDetailsView;
class FStructOnScope;
struct FPropertyChangedEvent;

/**
 * Popover body editing a property's Choices, or an import row's choices patch. Edits land on a scratch copy; every
 * finished change commits the copy through the real handle in one transaction (undo, dirty, and the host's own
 * post-edit path, which re-mirrors every row), then the copy is refreshed from what the host made of it.
 */
class SPCGExPropertyChoicesEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExPropertyChoicesEditor)
			: _bPatch(false)
		{
		}

		/** The real FPCGExPropertyChoices (or FPCGExPropertyChoicesPatch) handle the copy commits through. */
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, RealHandle)
		SLATE_ARGUMENT(FPCGExChoicesEditContext, Context)
		SLATE_ARGUMENT(bool, bPatch)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void* GetRealData() const;
	void OnFinishedChanging(const FPropertyChangedEvent& InEvent);
	void RefreshScratchFromReal();

	TSharedPtr<IPropertyHandle> RealHandle;
	FPCGExChoicesEditContext Context;
	bool bPatch = false;

	const UScriptStruct* MemberStruct = nullptr;
	TSharedPtr<FStructOnScope> Scratch;
	/** The edited struct inside the scratch root; the registry key. */
	void* ScratchMember = nullptr;
	TUniquePtr<FPCGExChoicesEditScope> Scope;
	TSharedPtr<IStructureDetailsView> View;
	bool bCommitting = false;
};
