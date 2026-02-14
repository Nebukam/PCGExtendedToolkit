// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UPCGExValencyCageEditorMode;
class APCGExValencyCageBase;
class UPCGExValencyCageConnectorComponent;

/**
 * Context-sensitive inspector panel.
 * Shows different content based on what's selected:
 * - Nothing: Scene stats summary
 * - Cage: Orbital status, asset count, connector list
 * - Connector component: Connector properties
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

	/** Scene changed delegate handle */
	FDelegateHandle OnSceneChangedHandle;

	/** Persisted search filter text for connector lists */
	FString ConnectorSearchFilter;

	/** When set, RefreshContent stays on the connector detail panel instead of redirecting to cage view */
	TWeakObjectPtr<UPCGExValencyCageConnectorComponent> DetailPanelConnector;

	/** Guard: when true, RefreshContent is deferred until selection updates complete */
	bool bIsUpdatingSelection = false;

	/** Rebuild content based on current selection */
	void RefreshContent();

	/** Selection changed callback (delegate compatible) */
	void OnSelectionChangedCallback(UObject* InObject);

	/** Scene changed callback — refreshes when cages/volumes/palettes are added/removed */
	void OnSceneChangedCallback();

	/** Build content widgets for each context */
	TSharedRef<SWidget> BuildSceneStatsContent();
	TSharedRef<SWidget> BuildCageContent(class APCGExValencyCageBase* Cage);
	TSharedRef<SWidget> BuildConnectorContent(class UPCGExValencyCageConnectorComponent* Connector);
	TSharedRef<SWidget> BuildVolumeContent(class AValencyContextVolume* Volume);
	TSharedRef<SWidget> BuildPaletteContent(class APCGExValencyAssetPalette* Palette);

	/** Build a compact connector row with inline controls */
	TSharedRef<SWidget> MakeCompactConnectorRow(UPCGExValencyCageConnectorComponent* Connector, bool bIsActive);

	/** Build the Add Connector button */
	TSharedRef<SWidget> MakeAddConnectorButton(APCGExValencyCageBase* Cage);

	/** Build a Rebuild All button */
	TSharedRef<SWidget> MakeRebuildAllButton();

	/** Build contextual related actors section */
	TSharedRef<SWidget> MakeRelatedSection(APCGExValencyCageBase* Cage);

	/** Helper to create a labeled row */
	static TSharedRef<SWidget> MakeLabeledRow(const FText& Label, const FText& Value);
	static TSharedRef<SWidget> MakeLabeledColorRow(const FText& Label, const FLinearColor& Color);
	static TSharedRef<SWidget> MakeSectionHeader(const FText& Title);
};
