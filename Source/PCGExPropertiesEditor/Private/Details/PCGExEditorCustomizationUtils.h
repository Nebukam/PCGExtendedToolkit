// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "PropertyHandle.h"

namespace PCGExEditorCustomizationUtils
{
	// External-FStructOnScope rows write directly to struct memory and bypass the owner's
	// PreEditChange / PostEditChangeProperty pipeline. Hook the row to stand in for it:
	//  - pre-change: Modify() the owner. FPropertyNode::NotifyPreChange reaches no object for these rows, and a
	//    Modify() made after the write snapshots the edited state, so the transaction would undo nothing.
	//  - post-change: broadcast OnObjectPropertyChanged directly -- the signal PCG asset tracking listens to.
	//    Routing through PostEditChangeProperty instead would trigger the heavy structural-rebuild path for
	//    what is just a value commit. Interactive ticks are skipped (hence the WithData setter): a drag in
	//    progress shouldn't tick-storm every listener of that global delegate.
	//
	// Registered on both the own- and child- delegates: multi-component widgets
	// (Vector / Rotator) commit through their X/Y/Z child handles, which fire only the parent's
	// child delegates (FPropertyNode::BroadcastPropertyChangedDelegates), never its own --
	// so without the child registration those rows never dirty the owner. No double-fire: one edit
	// hits one node, and no call site hooks a node together with one of its ancestors.
	inline void HookOwnerChangeOnHandleChanged(
		const TSharedPtr<IPropertyHandle>& Handle,
		const TWeakObjectPtr<UObject>& WeakOwner)
	{
		if (!Handle.IsValid())
		{
			return;
		}

		const FSimpleDelegate OnPreChange = FSimpleDelegate::CreateLambda([WeakOwner]()
		{
			if (UObject* Live = WeakOwner.Get())
			{
				Live->Modify();
			}
		});

		Handle->SetOnPropertyValuePreChange(OnPreChange);
		Handle->SetOnChildPropertyValuePreChange(OnPreChange);

		// One handler registered on both delegates (the setters copy it).
		const TDelegate<void(const FPropertyChangedEvent&)> OnChanged =
			TDelegate<void(const FPropertyChangedEvent&)>::CreateLambda(
				[WeakOwner](const FPropertyChangedEvent& InEvent)
				{
					if (InEvent.ChangeType == EPropertyChangeType::Interactive)
					{
						return;
					}
					UObject* Live = WeakOwner.Get();
					if (!Live)
					{
						return;
					}
					// Covers a write that only raised the post-change side; a no-op once the pre-change hook ran.
					Live->Modify();
					FPropertyChangedEvent Event(InEvent.Property, EPropertyChangeType::ValueSet);
					FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Live, Event);
				});

		Handle->SetOnPropertyValueChangedWithData(OnChanged);
		Handle->SetOnChildPropertyValueChangedWithData(OnChanged);
	}
}
