// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExCollectionEditor.h"

#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Collections/PCGExAssetCollection.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SUniformGridPanel.h"

namespace PCGExCollectionEditor
{
}

void FPCGExAssetCollectionEditor::InitEditor(UPCGExAssetCollection* InCollection, const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	EditedCollection = InCollection;

	const TArray<UObject*> ObjectsToEdit = {InCollection};
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	TSharedRef<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			GetToolkitCommands(),
			FToolBarExtensionDelegate::CreateSP(this, &FPCGExAssetCollectionEditor::FillToolbar)
		);
	AddToolbarExtender(ToolbarExtender);

#pragma region InitAssetEditor

	// Property editor module
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	// Details view arguments
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NotifyHook = nullptr;
	DetailsArgs.bAllowMultipleTopLevelObjects = false;

	// Create the details view
	DetailsView = PropertyModule.CreateDetailView(DetailsArgs);

	// Set the asset to display
	DetailsView->SetObject(InCollection);

	// Create a layout
	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("PCGExAssetCollectionEditor_Layout")
		->AddArea(
				FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split(
						FTabManager::NewStack()
						->AddTab(DetailsViewTabId, ETabState::OpenedTab)
					)
			);

	InitAssetEditor(EToolkitMode::Standalone, InitToolkitHost, FName("PCGExAssetCollectionEditor"), Layout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);


#pragma endregion


	RegenerateMenusAndToolbars();
}

UPCGExAssetCollection* FPCGExAssetCollectionEditor::GetEditedCollection() const
{
	return EditedCollection.Get();
}

void FPCGExAssetCollectionEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
#define PCGEX_SLATE_ICON(_NAME) FSlateIcon(FAppStyle::GetAppStyleSetName(), "PCGEx.ActionIcon."#_NAME)
#define PCGEX_CURRENT_COLLECTION if (UPCGExAssetCollection* Collection = EditedCollection.Get())

#define PCGEX_SECTION_HEADER(_LABEL) \
	ToolbarBuilder.AddWidget(\
		SNew(SBox).VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(FMargin(8, 0))[\
			SNew(STextBlock).Text(INVTEXT(_LABEL)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(FSlateColor(FLinearColor(1, 1, 1, 0.8)))\
			.Justification(ETextJustify::Center)]);

	// -------------------- Append Section --------------------
	ToolbarBuilder.BeginSection("AppendSection");
	{
		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_AddBrowserSelection(); }
							})
					),
				NAME_None,
				FText::GetEmpty(),
				INVTEXT("Append the current content browser' selection to this collection."),
				PCGEX_SLATE_ICON(AddContentBrowserSelection)
			);
	}
	ToolbarBuilder.EndSection();

	// -------------------- Staging Section --------------------
	ToolbarBuilder.BeginSection("StagingSection");
	{
		PCGEX_SECTION_HEADER("Rebuild\nStaging")

		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_RebuildStagingData(); }
							})
					),
				NAME_None,
				FText::GetEmpty(),
				INVTEXT("Rebuild Staging for this asset collection."),
				PCGEX_SLATE_ICON(RebuildStaging)
			);

		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_RebuildStagingData_Recursive(); }
							})
					),
				NAME_None,
				FText::GetEmpty(),
				INVTEXT("Rebuild staging recursively (this and all subcollections)."),
				PCGEX_SLATE_ICON(RebuildStagingRecursive)
			);

		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_RebuildStagingData_Project(); }
							})
					),
				NAME_None,
				FText::GetEmpty(),
				INVTEXT("Rebuild staging for the entire project."),
				PCGEX_SLATE_ICON(RebuildStagingProject)
			);

		ToolbarBuilder.AddSeparator();
	}
	ToolbarBuilder.EndSection();

	// -------------------- Weighting Section --------------------
	ToolbarBuilder.BeginSection("WeightSection");
	{
		PCGEX_SECTION_HEADER("Weights")

		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_NormalizedWeightToSum(); }
							})
					),
				NAME_None,
				FText::GetEmpty(),
				INVTEXT("Normalize weight sum to 100"),
				PCGEX_SLATE_ICON(NormalizeWeight)
			);

		ToolbarBuilder.AddWidget(
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(2, 2))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("= i")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_SetWeightIndex(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Set the weight index to the entry index."))
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("100")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_WeightOne(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Reset all weights to 100"))
				]
				+ SUniformGridPanel::Slot(2, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("+=1")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_PadWeight(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Add 1 to all weights"))
				]
				// Row 2
				+ SUniformGridPanel::Slot(0, 1)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("×2")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_MultWeight2(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Multiply weights by 2"))
				]
				+ SUniformGridPanel::Slot(1, 1)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("×10")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_MultWeight10(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Multiply weights by 10"))
				]
				+ SUniformGridPanel::Slot(2, 1)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("???")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_WeightRandom(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Assign random weights"))
				]
			);
	}
	ToolbarBuilder.EndSection();

	// -------------------- Sorting Section --------------------
	ToolbarBuilder.BeginSection("SortingSection");
	{
		ToolbarBuilder.AddWidget(
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(1, 2))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("▲")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_SortByWeightAscending(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Sort collection by ascending weight"))
				]
				+ SUniformGridPanel::Slot(0, 1)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("▼")))
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_SortByWeightDescending(); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Sort collection by descending weight"))
				]
			);
	}
	ToolbarBuilder.EndSection();
}

void FPCGExAssetCollectionEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	// Register tab spawner with out layout Id
	InTabManager->RegisterTabSpawner(
			            DetailsViewTabId,
			            FOnSpawnTab::CreateLambda(
				            [this](const FSpawnTabArgs& Args)
				            {
					            return SNew(SDockTab)
						            .TabRole(ETabRole::PanelTab)
						            [
							            DetailsView.ToSharedRef()
						            ];
				            })
		            )
	            .SetDisplayName(INVTEXT("Details"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}
