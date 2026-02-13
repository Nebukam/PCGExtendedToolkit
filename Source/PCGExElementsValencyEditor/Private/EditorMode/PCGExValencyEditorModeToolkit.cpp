// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyEditorModeToolkit.h"

#include "EditorModeManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SCheckBox.h"

#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "Widgets/SValencyVisToggles.h"
#include "Widgets/SValencySceneOverview.h"
#include "Widgets/SValencyInspector.h"
#include "Widgets/SValencyValidation.h"

#define LOCTEXT_NAMESPACE "ValencyEditor"

#pragma region FValencyEditorCommands

void FValencyEditorCommands::RegisterCommands()
{
	UI_COMMAND(CleanupConnections, "Cleanup Connections", "Remove stale manual connections from all cages", EUserInterfaceActionType::Button, FInputChord(EKeys::C, EModifierKey::Control | EModifierKey::Shift));
	UI_COMMAND(AddConnector, "Add Connector", "Add a new connector to the selected cage", EUserInterfaceActionType::Button, FInputChord(EKeys::A, EModifierKey::Control | EModifierKey::Shift));
	UI_COMMAND(RemoveConnector, "Remove Connector", "Remove the selected connector component", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
	UI_COMMAND(DuplicateConnector, "Duplicate Connector", "Duplicate the selected connector with offset", EUserInterfaceActionType::Button, FInputChord(EKeys::D, EModifierKey::Control));
	UI_COMMAND(CycleConnectorPolarity, "Cycle Connector Polarity", "Cycle polarity: Universal, Plug, Port", EUserInterfaceActionType::Button, FInputChord(EKeys::D, EModifierKey::Control | EModifierKey::Shift));
}

#pragma endregion

#pragma region SValencyModePanel

void SValencyModePanel::Construct(const FArguments& InArgs)
{
	EditorMode = InArgs._EditorMode;

	ChildSlot
	[
		SAssignNew(ScrollBox, SScrollBox)
	];

	RebuildLayout();

	// Bind to scene changes if we have an editor mode
	if (EditorMode)
	{
		EditorMode->OnSceneChanged.AddSP(this, &SValencyModePanel::RefreshPanel);
	}
}

void SValencyModePanel::RefreshPanel()
{
	RebuildLayout();
}

void SValencyModePanel::RebuildLayout()
{
	if (!ScrollBox.IsValid())
	{
		return;
	}

	ScrollBox->ClearChildren();

	// Visualization toggles section
	ScrollBox->AddSlot()
	.Padding(4.0f)
	[
		SAssignNew(VisTogglesWidget, SValencyVisToggles)
		.EditorMode(EditorMode)
	];

	ScrollBox->AddSlot()
	.Padding(2.0f, 0.0f)
	[
		SNew(SSeparator)
	];

	// Scene overview section
	ScrollBox->AddSlot()
	.Padding(4.0f)
	[
		SAssignNew(SceneOverviewWidget, SValencySceneOverview)
		.EditorMode(EditorMode)
	];

	ScrollBox->AddSlot()
	.Padding(2.0f, 0.0f)
	[
		SNew(SSeparator)
	];

	// Context-sensitive inspector section
	ScrollBox->AddSlot()
	.Padding(4.0f)
	[
		SAssignNew(InspectorWidget, SValencyInspector)
		.EditorMode(EditorMode)
	];

	ScrollBox->AddSlot()
	.Padding(2.0f, 0.0f)
	[
		SNew(SSeparator)
	];

	// Validation section
	ScrollBox->AddSlot()
	.Padding(4.0f)
	[
		SAssignNew(ValidationWidget, SValencyValidation)
		.EditorMode(EditorMode)
	];
}

#pragma endregion

#pragma region FPCGExValencyEditorModeToolkit

FPCGExValencyEditorModeToolkit::FPCGExValencyEditorModeToolkit()
{
}

FPCGExValencyEditorModeToolkit::~FPCGExValencyEditorModeToolkit()
{
}

void FPCGExValencyEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
	EnsurePanelCreated();
}

void FPCGExValencyEditorModeToolkit::EnsurePanelCreated()
{
	if (PanelWidget.IsValid())
	{
		return;
	}

	// Get the editor mode via the UEdMode owning mode
	UPCGExValencyCageEditorMode* ValencyMode = Cast<UPCGExValencyCageEditorMode>(GetScriptableEditorMode());

	SAssignNew(PanelWidget, SValencyModePanel)
		.EditorMode(ValencyMode);
}

FName FPCGExValencyEditorModeToolkit::GetToolkitFName() const
{
	return FName("PCGExValencyEditorModeToolkit");
}

FText FPCGExValencyEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("PCGExValency", "ToolkitName", "Valency");
}

TSharedPtr<SWidget> FPCGExValencyEditorModeToolkit::GetInlineContent() const
{
	// Lazy creation: ensure panel exists when queried
	const_cast<FPCGExValencyEditorModeToolkit*>(this)->EnsurePanelCreated();
	return PanelWidget;
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
