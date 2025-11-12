// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExAssetCollectionEditor.h"

#include "PCGExGlobalEditorSettings.h"
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

TMap<FName, PCGExAssetCollectionEditor::FilterInfos> FPCGExAssetCollectionEditor::FilterInfos;

FPCGExAssetCollectionEditor::FPCGExAssetCollectionEditor()
{
	RegisterPropertyNameMapping(GetMutableDefault<UPCGExGlobalEditorSettings>()->PropertyNamesMap);
}

void FPCGExAssetCollectionEditor::InitEditor(UPCGExAssetCollection* InCollection, const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	EditedCollection = InCollection;

	const TArray<UObject*> ObjectsToEdit = {InCollection};
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	// Create a layout
	TSharedRef<FTabManager::FArea> Area =
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Horizontal);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("PCGExAssetCollectionEditor_Layout_v1")
		->AddArea(Area);

	CreateTabs(Tabs);

	for (int i = 0; i < Tabs.Num(); i++)
	{
		Area->Split
			(
				FTabManager::NewStack()
				->AddTab(Tabs[i].Id, ETabState::OpenedTab)
				->SetForegroundTab(Tabs[i].Id)
			);
	}

	InitAssetEditor(EToolkitMode::Standalone, InitToolkitHost, FName("PCGExAssetCollectionEditor"), Layout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsToEdit);

	// Toolbar extender
	TSharedRef<FExtender> ToolbarExtender = MakeShared<FExtender>();
	ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			GetToolkitCommands(),
			FToolBarExtensionDelegate::CreateSP(this, &FPCGExAssetCollectionEditor::BuildEditorToolbar)
		);

	AddToolbarExtender(ToolbarExtender);
	RegenerateMenusAndToolbars();

	// Assuming DetailsView is already created.
	UPCGExGlobalEditorSettings::OnHiddenAssetPropertyNamesChanged.AddRaw(this, &FPCGExAssetCollectionEditor::ForceRefreshTabs);
}

UPCGExAssetCollection* FPCGExAssetCollectionEditor::GetEditedCollection() const
{
	return EditedCollection.Get();
}

void FPCGExAssetCollectionEditor::RegisterPropertyNameMapping(TMap<FName, FName>& Mapping)
{
#define PCGEX_DECL_ASSET_FILTER(_NAME, _ID, _LABEL, _TOOLTIP)PCGExAssetCollectionEditor::FilterInfos& _NAME = FilterInfos.Emplace(FName(_ID), PCGExAssetCollectionEditor::FilterInfos(FName(_ID),FTEXT(_LABEL), FTEXT(_TOOLTIP)));

	PCGEX_DECL_ASSET_FILTER(Variations, "AssetEditor.Variations", "Variations", "Show/hide Variations")
	Mapping.Add(FName("VariationMode"), Variations.Id);
	Mapping.Add(FName("Variations"), Variations.Id);

	PCGEX_DECL_ASSET_FILTER(Tags, "AssetEditor.Tags", "Tags", "Show/hide Tags")
	Mapping.Add(FName("Tags"), Tags.Id);

	PCGEX_DECL_ASSET_FILTER(Staging, "AssetEditor.Staging", "Staging", "Show/hide Staging")
	Mapping.Add(FName("Staging"), Staging.Id);

	PCGEX_DECL_ASSET_FILTER(Materials, "AssetEditor.Materials", "Materials", "Show/hide Materials")	
	Mapping.Add(FName("MaterialVariants"), Materials.Id);
	Mapping.Add(FName("SlotIndex"), Materials.Id);
	Mapping.Add(FName("MaterialOverrideVariants"), Materials.Id);
	Mapping.Add(FName("MaterialOverrideVariantsList"), Materials.Id);
	
	PCGEX_DECL_ASSET_FILTER(Descriptors, "AssetEditor.Descriptors", "Descriptors", "Show/hide Descriptors")	
	Mapping.Add(FName("DescriptorSource"), Descriptors.Id);
	Mapping.Add(FName("ISMDescriptor"), Descriptors.Id);
	Mapping.Add(FName("SMDescriptor"), Descriptors.Id);

#undef PCGEX_DECL_ASSET_FILTER
}

void FPCGExAssetCollectionEditor::CreateTabs(TArray<PCGExAssetCollectionEditor::TabInfos>& OutTabs)
{
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
	TSharedPtr<IDetailsView> DetailsView = PropertyModule.CreateDetailView(DetailsArgs);
	DetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateLambda(
			[](const FPropertyAndParent& PropertyAndParent)
			{
				return PropertyAndParent.Property.GetFName() != TEXT("Entries");
			}));

	// Set the asset to display
	DetailsView->SetObject(EditedCollection.Get());

	PCGExAssetCollectionEditor::TabInfos& Infos = OutTabs.Emplace_GetRef(FName("Collection"), DetailsView, FName("Collection Settings"));
	Infos.Icon = TEXT("Settings");
}

#define PCGEX_SLATE_ICON(_NAME) FSlateIcon(FAppStyle::GetAppStyleSetName(), "PCGEx.ActionIcon."#_NAME)
#define PCGEX_CURRENT_COLLECTION if (UPCGExAssetCollection* Collection = EditedCollection.Get())
#define PCGEX_SECTION_HEADER(_LABEL) \
ToolbarBuilder.AddWidget(\
SNew(SBox).VAlign(VAlign_Center).HAlign(HAlign_Center).Padding(FMargin(8, 0))[\
SNew(STextBlock).Text(INVTEXT(_LABEL)).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(FSlateColor(FLinearColor(1, 1, 1, 0.8)))\
.Justification(ETextJustify::Center)]);

void FPCGExAssetCollectionEditor::BuildEditorToolbar(FToolBarBuilder& ToolbarBuilder)
{
#pragma region Staging

	ToolbarBuilder.BeginSection("StagingSection");
	{
		//PCGEX_SECTION_HEADER("Rebuild\nStaging")

		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_RebuildStagingData(); }
							})
					),
				NAME_None,
				FText::FromString(TEXT("Rebuild")),
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
				FText::GetEmpty(), //FText::FromString(TEXT("Rebuild (Recursive)")),
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
				FText::GetEmpty(), //FText::FromString(TEXT("Rebuild All (Project)")),
				INVTEXT("Rebuild staging for the entire project. (Will go through all collection assets)"),
				PCGEX_SLATE_ICON(RebuildStagingProject)
			);

		ToolbarBuilder.AddSeparator();
	}
	ToolbarBuilder.EndSection();

#pragma endregion
}

void FPCGExAssetCollectionEditor::BuildAssetHeaderToolbar(FToolBarBuilder& ToolbarBuilder)
{
#pragma region Append


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

#pragma endregion

#pragma region Weighting

	ToolbarBuilder.BeginSection("WeightSection");
	{
		PCGEX_SECTION_HEADER("Weight")

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

#pragma endregion

#pragma region Sorting


	ToolbarBuilder.BeginSection("SortingSection");
	{
		PCGEX_SECTION_HEADER("Sort")

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

#pragma endregion
}

void FPCGExAssetCollectionEditor::BuildAssetFooterToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("FilterSection");
	{
		PCGEX_SECTION_HEADER("Filters")

		TSharedRef<SUniformGridPanel> Grid =
			SNew(SUniformGridPanel)
			.SlotPadding(FMargin(2, 2));

		int32 Index = 0;
		for (const TPair<FName, PCGExAssetCollectionEditor::FilterInfos>& Infos : FilterInfos)
		{
			const PCGExAssetCollectionEditor::FilterInfos& Filter = Infos.Value;

			Grid->AddSlot(Index / 2, Index % 2)
			[
				SNew(SButton)
				.Text(Filter.Label)
				.ButtonStyle(FAppStyle::Get(), "PCGEx.ActionIcon")
				.OnClicked_Lambda(
					[Filter]()
					{
						UPCGExGlobalEditorSettings* MutableSettings = GetMutableDefault<UPCGExGlobalEditorSettings>();
						MutableSettings->ToggleHiddenAssetPropertyName(Filter.Id, MutableSettings->GetIsPropertyVisible(Filter.Id));
						return FReply::Handled();
					})
				.ButtonColorAndOpacity_Lambda(
					[Filter]
					{
						return GetMutableDefault<UPCGExGlobalEditorSettings>()->GetIsPropertyVisible(Filter.Id) ? FLinearColor(0.005, 0.005, 0.005, 0.8) : FLinearColor::Transparent;
					})
				.ToolTipText(Filter.ToolTip)
			];

			Index++;
		}

		ToolbarBuilder.AddWidget(Grid);
	}
	ToolbarBuilder.EndSection();
}

#undef PCGEX_SLATE_ICON
#undef PCGEX_CURRENT_COLLECTION
#undef PCGEX_SECTION_HEADER

void FPCGExAssetCollectionEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	TabManager->SetCanDoDragOperation(false);

	for (const PCGExAssetCollectionEditor::TabInfos& Tab : Tabs)
	{
		// Register tab spawner with out layout Id
		FTabSpawnerEntry& Entry =
			TabManager->RegisterTabSpawner(
					          Tab.Id,
					          FOnSpawnTab::CreateLambda(
						          [Tab](const FSpawnTabArgs& Args)
						          {
							          return SNew(SDockTab)
								          .TabRole(Tab.Role)
								          .CanEverClose(false)
								          [
									          SNew(SVerticalBox)
									          + SVerticalBox::Slot()
									          .AutoHeight()
									          [
										          Tab.Header.IsValid() ? Tab.Header.ToSharedRef() : SNullWidget::NullWidget
									          ]
									          + SVerticalBox::Slot()
									          .FillHeight(1.f)
									          [
										          Tab.View.ToSharedRef()
									          ]
									          + SVerticalBox::Slot()
									          .AutoHeight()
									          [
										          Tab.Footer.IsValid() ? Tab.Footer.ToSharedRef() : SNullWidget::NullWidget
									          ]
								          ];
						          })
				          )
			          .SetDisplayName(FText::FromName(Tab.Label));

		if (!Tab.Icon.IsEmpty())
		{
			FString Icon = TEXT("PCGEx.ActionIcon.") + Tab.Icon;
			Entry.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), FName(Icon)));
		}
	}

	if (!Tabs.IsEmpty()) { TabManager->SetMainTab(Tabs[0].Id); }

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FPCGExAssetCollectionEditor::ForceRefreshTabs()
{
	for (const PCGExAssetCollectionEditor::TabInfos& Tab : Tabs)
	{
		if (TSharedPtr<IDetailsView> DetailsView = StaticCastSharedPtr<IDetailsView>(Tab.View))
		{
			DetailsView->ForceRefresh();
		}
	}
}
