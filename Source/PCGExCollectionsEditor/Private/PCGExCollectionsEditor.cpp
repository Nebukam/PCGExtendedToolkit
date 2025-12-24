// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCollectionsEditor.h"

#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "PCGExAssetTypesMacros.h"
#include "PCGExCollectionsEditorMenuUtils.h"
#include "Details/Collections/PCGExActorCollectionActions.h"
#include "Details/Collections/PCGExAssetEntryCustomization.h"
#include "Details/Collections/PCGExAssetGrammarCustomization.h"
#include "Details/Collections/PCGExFittingVariationsCustomization.h"
#include "Details/Collections/PCGExMaterialPicksCustomization.h"
#include "Details/Collections/PCGExMeshCollectionActions.h"

#define LOCTEXT_NAMESPACE "FPCGExCollectionsEditorModule"

#undef LOCTEXT_NAMESPACE

void FPCGExCollectionsEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShared<FPCGExMeshCollectionActions>());
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MakeShared<FPCGExActorCollectionActions>());

	PCGEX_REGISTER_CUSTO_START

	PCGEX_REGISTER_CUSTO("PCGExFittingVariations", FPCGExFittingVariationsCustomization)
	PCGEX_REGISTER_CUSTO("PCGExMaterialOverrideEntry", FPCGExMaterialOverrideEntryCustomization)
	PCGEX_REGISTER_CUSTO("PCGExMaterialOverrideSingleEntry", FPCGExMaterialOverrideSingleEntryCustomization)
	PCGEX_REGISTER_CUSTO("PCGExMaterialOverrideCollection", FPCGExMaterialOverrideCollectionCustomization)
	PCGEX_REGISTER_CUSTO("PCGExMeshCollectionEntry", FPCGExMeshEntryCustomization)
	PCGEX_REGISTER_CUSTO("PCGExActorCollectionEntry", FPCGExActorEntryCustomization)
	PCGEX_REGISTER_CUSTO("PCGExAssetGrammarDetails", FPCGExAssetGrammarCustomization)
}

void FPCGExCollectionsEditorModule::RegisterMenuExtensions()
{
	IPCGExEditorModuleInterface::RegisterMenuExtensions();

	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* WorldAssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.AssetActionsSubMenu"))
	{
		// Use a dynamic section here because we might have plugins registering at a later time
		FToolMenuSection& Section = WorldAssetMenu->AddDynamicSection(
			"PCGEx", FNewToolMenuDelegate::CreateLambda(
				[this](UToolMenu* ToolMenu)
				{
					if (!GEditor || GEditor->GetPIEWorldContext() || !ToolMenu) { return; }
					if (UContentBrowserAssetContextMenuContext* AssetMenuContext = ToolMenu->Context.FindContext<UContentBrowserAssetContextMenuContext>())
					{
						PCGExCollectionsEditorMenuUtils::CreateOrUpdatePCGExAssetCollectionsFromMenu(ToolMenu, AssetMenuContext->SelectedAssets);
					}
				}), FToolMenuInsert(NAME_None, EToolMenuInsertType::Default));
	}
}

PCGEX_IMPLEMENT_MODULE(FPCGExCollectionsEditorModule, PCGExCollectionsEditor)