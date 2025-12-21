// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExActorCollectionEditor.h"

#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Core/PCGExAssetCollection.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"

FPCGExActorCollectionEditor::FPCGExActorCollectionEditor()
	: FPCGExAssetCollectionEditor()
{
}

void FPCGExActorCollectionEditor::BuildAssetHeaderToolbar(FToolBarBuilder& ToolbarBuilder)
{
	FPCGExAssetCollectionEditor::BuildAssetHeaderToolbar(ToolbarBuilder);
}

void FPCGExActorCollectionEditor::CreateTabs(TArray<PCGExAssetCollectionEditor::TabInfos>& OutTabs)
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
	PCGExAssetCollectionEditor::TabInfos& Infos = OutTabs.Emplace_GetRef(FName("Assets"), DetailsView);
	Infos.Icon = TEXT("Entries");

	FToolBarBuilder HeaderToolbarBuilder(GetToolkitCommands(), FMultiBoxCustomization::None);
	HeaderToolbarBuilder.SetStyle(&FAppStyle::Get(), FName("Toolbar"));
	BuildAssetHeaderToolbar(HeaderToolbarBuilder);
	Infos.Header = HeaderToolbarBuilder.MakeWidget();

	FToolBarBuilder FooterToolbarBuilder(GetToolkitCommands(), FMultiBoxCustomization::None);
	FooterToolbarBuilder.SetStyle(&FAppStyle::Get(), FName("Toolbar"));
	BuildAssetFooterToolbar(FooterToolbarBuilder);
	Infos.Footer = FooterToolbarBuilder.MakeWidget();
}
