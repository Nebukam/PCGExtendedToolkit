// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UPCGExValencyCageEditorMode;
class APCGExValencyCageBase;
class UPCGExValencyCageSocketComponent;

/**
 * Context-sensitive inspector panel.
 * Shows different content based on what's selected:
 * - Nothing: Scene stats summary
 * - Cage: Orbital status, asset count, socket list
 * - Socket component: Socket properties
 * - Volume: Contained cages, bonding rules, compile status
 * - Palette: Asset count, mirroring info
 */
class SValencyInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValencyInspector) {}
		SLATE_ARGUMENT(UPCGExValencyCageEditorMode*, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	UPCGExValencyCageEditorMode* EditorMode = nullptr;

	/** Content switcher */
	TSharedPtr<SWidget> ContentArea;

	/** Selection change delegate handle */
	FDelegateHandle OnSelectionChangedHandle;

	/** Component selection change delegate handle */
	FDelegateHandle OnComponentSelectionChangedHandle;

	/** Rebuild content based on current selection */
	void RefreshContent();

	/** Selection changed callback (delegate compatible) */
	void OnSelectionChangedCallback(UObject* InObject);

	/** Build content widgets for each context */
	TSharedRef<SWidget> BuildSceneStatsContent();
	TSharedRef<SWidget> BuildCageContent(class APCGExValencyCageBase* Cage);
	TSharedRef<SWidget> BuildSocketContent(class UPCGExValencyCageSocketComponent* Socket);
	TSharedRef<SWidget> BuildVolumeContent(class AValencyContextVolume* Volume);
	TSharedRef<SWidget> BuildPaletteContent(class APCGExValencyAssetPalette* Palette);

	/** Build an interactive socket row with inline controls */
	TSharedRef<SWidget> MakeSocketRow(UPCGExValencyCageSocketComponent* Socket);

	/** Build the Add Socket button */
	TSharedRef<SWidget> MakeAddSocketButton(APCGExValencyCageBase* Cage);

	/** Helper to create a labeled row */
	static TSharedRef<SWidget> MakeLabeledRow(const FText& Label, const FText& Value);
	static TSharedRef<SWidget> MakeLabeledColorRow(const FText& Label, const FLinearColor& Color);
	static TSharedRef<SWidget> MakeSectionHeader(const FText& Title);
};
