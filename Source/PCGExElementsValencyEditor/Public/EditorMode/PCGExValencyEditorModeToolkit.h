// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SScrollBox.h"

class FPCGExValencyCageEditorMode;

/**
 * Root panel widget for the Valency editor mode side panel.
 * Contains all sub-sections in a scrollable layout:
 * - Visualization toggles
 * - Scene overview tree
 * - Context-sensitive inspector
 * - Validation messages
 */
class SValencyModePanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValencyModePanel) {}
		SLATE_ARGUMENT(FPCGExValencyCageEditorMode*, EditorMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Get the editor mode this panel operates on */
	FPCGExValencyCageEditorMode* GetEditorMode() const { return EditorMode; }

	/** Rebuild all panel sections (called when scene changes) */
	void RefreshPanel();

private:
	/** The editor mode we're attached to */
	FPCGExValencyCageEditorMode* EditorMode = nullptr;

	/** Root scroll box containing all sections */
	TSharedPtr<SScrollBox> ScrollBox;

	/** Sub-widgets (populated incrementally by phases) */
	TSharedPtr<SWidget> VisTogglesWidget;
	TSharedPtr<SWidget> SceneOverviewWidget;
	TSharedPtr<SWidget> InspectorWidget;
	TSharedPtr<SWidget> ValidationWidget;

	/** Rebuild the scroll box contents */
	void RebuildLayout();
};

/**
 * Mode toolkit for the Valency editor mode.
 * Creates and manages the side panel that appears when the mode is active.
 *
 * Supports both the UEdMode Init path (via FModeToolkit::Init) and direct
 * construction from FEdMode (via SetEditorMode + EnsurePanelCreated).
 */
class PCGEXELEMENTSVALENCYEDITOR_API FPCGExValencyEditorModeToolkit : public FModeToolkit
{
public:
	FPCGExValencyEditorModeToolkit();
	virtual ~FPCGExValencyEditorModeToolkit() override;

	//~ Begin FModeToolkit Interface
	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual TSharedPtr<SWidget> GetInlineContent() const override;
	//~ End FModeToolkit Interface

	/** Set the editor mode directly (for FEdMode path where Init is not called with a valid UEdMode) */
	void SetEditorMode(FPCGExValencyCageEditorMode* InMode);

	/** Ensure the panel widget is created (lazy initialization) */
	void EnsurePanelCreated();

	/** Get the panel widget */
	TSharedPtr<SValencyModePanel> GetPanelWidget() const { return PanelWidget; }

private:
	/** Cached editor mode pointer (for FEdMode path) */
	FPCGExValencyCageEditorMode* CachedEditorMode = nullptr;

	/** The root panel widget */
	TSharedPtr<SValencyModePanel> PanelWidget;
};
