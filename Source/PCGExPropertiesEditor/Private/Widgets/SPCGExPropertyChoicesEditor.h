// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FPCGExChoicesEditSession;
class FTransactionObjectEvent;
class IStructureDetailsView;
struct FPropertyChangedEvent;

/**
 * Popover body editing a property's Choices, or an import row's choices patch. Its details view shows the session's
 * scratch copy; every finished change is one FPCGExChoicesEditSession::Commit. The popover closes itself as soon as
 * the session can no longer reach what it edits.
 */
class SPCGExPropertyChoicesEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExPropertyChoicesEditor)
		{
		}

		SLATE_ARGUMENT(TSharedPtr<FPCGExChoicesEditSession>, Session)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPCGExPropertyChoicesEditor() override;

private:
	void OnFinishedChanging(const FPropertyChangedEvent& InEvent);
	void OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event);

	/** Rebuild the view's rows on the next tick; calls made before then fold into one. */
	void RequestRebuild();
	EActiveTimerReturnType CheckSession(double InCurrentTime, float InDeltaTime);
	void Dismiss();

	TSharedPtr<FPCGExChoicesEditSession> Session;
	TSharedPtr<IStructureDetailsView> View;
	FDelegateHandle TransactedHandle;
	bool bRebuildPending = false;
	bool bResyncPending = false;
};
