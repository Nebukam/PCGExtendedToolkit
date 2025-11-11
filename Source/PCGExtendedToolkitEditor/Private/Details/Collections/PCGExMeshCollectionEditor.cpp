// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExMeshCollectionEditor.h"

#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SUniformGridPanel.h"

void FPCGExMeshCollectionEditor::BuildAssetHeaderToolbar(FToolBarBuilder& ToolbarBuilder)
{
	FPCGExAssetCollectionEditor::BuildAssetHeaderToolbar(ToolbarBuilder);

#define PCGEX_SLATE_ICON(_NAME) FSlateIcon(FAppStyle::GetAppStyleSetName(), "PCGEx.ActionIcon."#_NAME)
#define PCGEX_CURRENT_COLLECTION if (UPCGExMeshCollection* Collection = Cast<UPCGExMeshCollection>(EditedCollection.Get()))

#pragma region Collision

	ToolbarBuilder.BeginSection("CollisionSection");
	{
		ToolbarBuilder.AddToolBarButton(
				FUIAction(
						FExecuteAction::CreateLambda(
							[&]()
							{
								PCGEX_CURRENT_COLLECTION { Collection->EDITOR_DisableCollisions(); }
							})
					),
				NAME_None,
				FText::GetEmpty(),
				INVTEXT("Disable collision on all assets within that collection."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "PhysicsAssetEditor.DisableCollisionAll")
			);
	}
	ToolbarBuilder.EndSection();

#pragma endregion

#pragma region Sorting


	ToolbarBuilder.BeginSection("DescriptorSection");
	{
		ToolbarBuilder.AddWidget(
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(1, 2))
				+ SUniformGridPanel::Slot(0, 0)
				[
					SNew(SButton)
					.Text(FText::GetEmpty())
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_SetDescriptorSourceAll(EPCGExEntryVariationMode::Global); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Set all entry Descriptor to \"Inherit from collection\". Each entry will inherit from the collection global descriptors.\nNOTE : Local settings are preserved, just hidden."))
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("PCGEx.ActionIcon.CollectionRule"))
					]
				]
				+ SUniformGridPanel::Slot(0, 1)
				[
					SNew(SButton)
					.Text(FText::GetEmpty())
					.OnClicked_Lambda(
						[&]()
						{
							PCGEX_CURRENT_COLLECTION { Collection->EDITOR_SetDescriptorSourceAll(EPCGExEntryVariationMode::Local); }
							return FReply::Handled();
						})
					.ToolTipText(FText::FromString("Set all entry Descriptor to \"Local\" -- each entry is responsible for managing its own descriptors.\nNOTE : This will restore previous local settings."))
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("PCGEx.ActionIcon.EntryRule"))
					]
				]
			);
	}
	ToolbarBuilder.EndSection();

#pragma endregion

#undef PCGEX_SLATE_ICON
#undef PCGEX_CURRENT_COLLECTION
}

void FPCGExMeshCollectionEditor::CreateTabs(TArray<FPCGExDetailsTabInfos>& OutTabs)
{
	// Default handling (will append default collection settings tab)
	FPCGExAssetCollectionEditor::CreateTabs(OutTabs);

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
				return PropertyAndParent.Property.GetFName() == PCGExAssetCollectionEditor::EntriesName
					|| (!PropertyAndParent.ParentProperties.IsEmpty() && PropertyAndParent.ParentProperties.Last()->GetFName() == PCGExAssetCollectionEditor::EntriesName);
			}));

	// Set the asset to display
	DetailsView->SetObject(EditedCollection.Get());
	FPCGExDetailsTabInfos& Infos = OutTabs.Emplace_GetRef(FName("Assets"), DetailsView);
	Infos.Icon = TEXT("Entries");

	FToolBarBuilder ToolbarBuilder(GetToolkitCommands(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), FName("Toolbar"));
	BuildAssetHeaderToolbar(ToolbarBuilder);
	Infos.Header = ToolbarBuilder.MakeWidget();
}
