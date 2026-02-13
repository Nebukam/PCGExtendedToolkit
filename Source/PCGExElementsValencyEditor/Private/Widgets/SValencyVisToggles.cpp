// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SValencyVisToggles.h"

#include "Editor.h"
#include "LevelEditorViewport.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

#include "EditorMode/PCGExValencyCageEditorMode.h"

void SValencyVisToggles::Construct(const FArguments& InArgs)
{
	EditorMode = InArgs._EditorMode;

	if (!EditorMode)
	{
		ChildSlot
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("PCGExValency", "NoMode", "No editor mode"))
		];
		return;
	}

	FValencyVisibilityFlags& Flags = EditorMode->GetMutableVisibilityFlags();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 2)
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("PCGExValency", "VisTogglesHeader", "Visibility"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SWrapBox)
			.UseAllottedSize(true)
			+ SWrapBox::Slot()
			.Padding(2)
			[
				MakeToggleButton(
					NSLOCTEXT("PCGExValency", "ToggleConnections", "Connections"),
					NSLOCTEXT("PCGExValency", "ToggleConnectionsTip", "Show orbital arrows and connection lines"),
					&Flags.bShowConnections)
			]
			+ SWrapBox::Slot()
			.Padding(2)
			[
				MakeToggleButton(
					NSLOCTEXT("PCGExValency", "ToggleLabels", "Labels"),
					NSLOCTEXT("PCGExValency", "ToggleLabelsTip", "Show cage names and orbital labels"),
					&Flags.bShowLabels)
			]
			+ SWrapBox::Slot()
			.Padding(2)
			[
				MakeToggleButton(
					NSLOCTEXT("PCGExValency", "ToggleSockets", "Sockets"),
					NSLOCTEXT("PCGExValency", "ToggleSocketsTip", "Show socket component diamonds"),
					&Flags.bShowSockets)
			]
			+ SWrapBox::Slot()
			.Padding(2)
			[
				MakeToggleButton(
					NSLOCTEXT("PCGExValency", "ToggleVolumes", "Volumes"),
					NSLOCTEXT("PCGExValency", "ToggleVolumesTip", "Show volume and palette wireframes"),
					&Flags.bShowVolumes)
			]
			+ SWrapBox::Slot()
			.Padding(2)
			[
				MakeToggleButton(
					NSLOCTEXT("PCGExValency", "ToggleGhosts", "Ghosts"),
					NSLOCTEXT("PCGExValency", "ToggleGhostsTip", "Show mirror/proxy ghost meshes"),
					&Flags.bShowGhostMeshes)
			]
			+ SWrapBox::Slot()
			.Padding(2)
			[
				MakeToggleButton(
					NSLOCTEXT("PCGExValency", "TogglePatterns", "Patterns"),
					NSLOCTEXT("PCGExValency", "TogglePatternsTip", "Show pattern bounds and proxy lines"),
					&Flags.bShowPatterns)
			]
		]
	];
}

TSharedRef<SWidget> SValencyVisToggles::MakeToggleButton(const FText& Label, const FText& Tooltip, bool* FlagPtr)
{
	return SNew(SCheckBox)
		.ToolTipText(Tooltip)
		.IsChecked_Lambda([FlagPtr]()
		{
			return *FlagPtr ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, FlagPtr](ECheckBoxState NewState)
		{
			*FlagPtr = (NewState == ECheckBoxState::Checked);
			RedrawViewports();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		];
}

void SValencyVisToggles::RedrawViewports() const
{
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}
}
