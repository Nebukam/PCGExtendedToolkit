// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Input/DragAndDrop.h"
#include "UObject/WeakObjectPtr.h"

class UPCGExAssetCollection;

/**
 * Custom drag-drop operation for collection grid tiles.
 * Carries the dragged entry indices, the source category, and the source collection so
 * cross-collection drops can resolve "which collection owns these indices" on the receiver.
 */
class FPCGExCollectionTileDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FPCGExCollectionTileDragDropOp, FDecoratedDragDropOp)

	/** Indices into the source collection's Entries array being dragged */
	TArray<int32> DraggedIndices;

	/** Category these entries originated from (in the source collection) */
	FName SourceCategory;

	/** Collection these indices reference. Required for cross-collection drop handling. */
	TWeakObjectPtr<UPCGExAssetCollection> SourceCollection;

	/**
	 * Authoritative copy/move flag. Set by the receiver at drop time from the drop event's
	 * Alt modifier; receivers then consult it. Same-collection drops ignore it.
	 */
	bool bIsCopyOperation = false;

	static TSharedRef<FPCGExCollectionTileDragDropOp> New(
		const TArray<int32>& InIndices,
		FName InSourceCategory,
		UPCGExAssetCollection* InSourceCollection);

	virtual void OnDragged(const FDragDropEvent& DragDropEvent) override
	{
		RefreshHoverText(DragDropEvent.IsAltDown());
		FDecoratedDragDropOp::OnDragged(DragDropEvent);
	}

private:
	void RefreshHoverText(const bool bIsCopy)
	{
		const FText Action = bIsCopy ? INVTEXT("Copy") : INVTEXT("Move");
		DefaultHoverText = FText::Format(
			INVTEXT("{0} {1} {1}|plural(one=entry,other=entries)"),
			Action,
			FText::AsNumber(DraggedIndices.Num()));
		CurrentHoverText = DefaultHoverText;
	}
};
