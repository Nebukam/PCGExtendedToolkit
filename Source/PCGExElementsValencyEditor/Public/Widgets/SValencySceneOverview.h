// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FPCGExValencyCageEditorMode;

/**
 * Entry in the scene overview list.
 */
struct FValencySceneEntry
{
	enum class EType : uint8
	{
		GroupHeader,
		Volume,
		Cage,
		Palette
	};

	EType Type = EType::GroupHeader;
	TWeakObjectPtr<AActor> Actor;
	FString DisplayName;
	FLinearColor IconColor = FLinearColor::White;
	int32 AssetCount = 0;
	int32 ConnectedOrbitals = 0;
	int32 TotalOrbitals = 0;
	bool bHasWarnings = false;
};

/**
 * Scene overview panel showing all cages, volumes, and palettes in a flat list.
 * Click to select, double-click to focus viewport on the actor.
 */
class SValencySceneOverview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValencySceneOverview) {}
		SLATE_ARGUMENT(FPCGExValencyCageEditorMode*, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rebuild the entry list from the editor mode's cached data */
	void RebuildList();

private:
	FPCGExValencyCageEditorMode* EditorMode = nullptr;

	/** List entries */
	TArray<TSharedPtr<FValencySceneEntry>> Entries;

	/** The list view widget */
	TSharedPtr<SListView<TSharedPtr<FValencySceneEntry>>> ListView;

	/** List view callbacks */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FValencySceneEntry> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnSelectionChanged(TSharedPtr<FValencySceneEntry> Item, ESelectInfo::Type SelectInfo);
	void OnDoubleClick(TSharedPtr<FValencySceneEntry> Item);

	/** Delegate handle for scene changes */
	FDelegateHandle OnSceneChangedHandle;
};
