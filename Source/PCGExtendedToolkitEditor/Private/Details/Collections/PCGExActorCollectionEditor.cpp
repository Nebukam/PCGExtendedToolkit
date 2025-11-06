// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExActorCollectionEditor.h"

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

void FPCGExActorCollectionEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	FPCGExAssetCollectionEditor::FillToolbar(ToolbarBuilder);
}

void FPCGExActorCollectionEditor::CreateTabs(TArray<FPCGExDetailsTabInfos>& OutTabs)
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
				return PropertyAndParent.Property.GetFName() == TEXT("Entries");
			}));
	
	// Set the asset to display
	DetailsView->SetObject(EditedCollection.Get());
	FPCGExDetailsTabInfos& Infos = OutTabs.Emplace_GetRef(FName("Assets"), DetailsView);
	Infos.Icon = TEXT("Entries");

	// Default handling (will append default collection tab)
	FPCGExAssetCollectionEditor::CreateTabs(OutTabs);
}
