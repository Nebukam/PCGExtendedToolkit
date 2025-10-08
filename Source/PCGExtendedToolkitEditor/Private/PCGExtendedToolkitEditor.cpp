﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "AssetRegistry/AssetData.h"
#include "EditorStyleSet.h"
#include "Editor.h"

#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "IAssetTools.h"
#include "PCGDataVisualizationRegistry.h"
#include "PCGExEditorMenuUtils.h"
#include "PCGExGlobalSettings.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Collections/PCGExActorCollectionActions.h"
#include "Collections/PCGExMeshCollectionActions.h"
#include "Collections/PCGExActorDataPackerActions.h"
#include "Data/PCGSpatialData.h"
#include "DataViz/PCGExSpatialDataVisualization.h"

// @SPLASH_DAMAGE_CHANGE [IMPROVEMENT] #SDTechArt - BEGIN: Moving LOCTEXT_NAMESPACE till after all includes as there is a collision with `MaterialLayersFunction`.
// Warning as follows:
// 		#define LOCTEXT_NAMESPACE "MaterialLayersFunctions"
// 		warning C4005: 'LOCTEXT_NAMESPACE': macro redefinition
// 		note: see previous definition of 'LOCTEXT_NAMESPACE'
// 		#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitEditorModule"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitEditorModule"

#define PCGEX_ADD_ICON(_NAME) \
Style->Set("ClassIcon." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeIcon));\
Style->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeThumbnail));

#define PCGEX_ADD_PIN_EXTRA_ICON(_NAME) \
AppStyle.Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGExPin_" #_NAME), TEXT(".svg")), SizePin));\
Style->Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGExPin_" #_NAME), TEXT(".svg")), SizePin));
// @SPLASH_DAMAGE_CHANGE [IMPROVEMENT] #SDTechArt - END: Moving LOCTEXT_NAMESPACE till after all includes as there is a collision with `MaterialLayersFunction`.

namespace PCGExEditor
{
	static FAutoConsoleCommand CommandListEditorOnlyGraphs(
		TEXT("pcgex.ListEditorOnlyGraphs"),
		TEXT("Finds all graph marked as IsEditorOnly."),
		FConsoleCommandDelegate::CreateLambda(
			[]()
			{
				const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

				FARFilter Filter;
				Filter.ClassPaths.Add(UPCGGraph::StaticClass()->GetClassPathName());
				Filter.bRecursiveClasses = true;

				TArray<FAssetData> AssetDataList;
				AssetRegistry.GetAssets(Filter, AssetDataList);

				if (AssetDataList.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("No Editor-only graph found."));
					return;
				}

				const int32 NumTotalGraphs = AssetDataList.Num();
				int32 NumEditorOnlyGraphs = 0;
				for (const FAssetData& AssetData : AssetDataList)
				{
					if (UPCGGraph* Graph = Cast<UPCGGraph>(AssetData.GetAsset()))
					{
						if (Graph->IsEditorOnly())
						{
							NumEditorOnlyGraphs++;
							UE_LOG(LogTemp, Warning, TEXT("%s"), *Graph->GetPathName());
						}
					}
				}

				UE_LOG(LogTemp, Warning, TEXT("Found %d EditorOnly graphs out of %d inspected graphs."), NumEditorOnlyGraphs, NumTotalGraphs);
			}));
}

void FPCGExtendedToolkitEditorModule::StartupModule()
{
	MeshCollectionActions = MakeShared<FPCGExMeshCollectionActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(MeshCollectionActions.ToSharedRef());
	
	ActorCollectionActions = MakeShared<FPCGExActorCollectionActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(ActorCollectionActions.ToSharedRef());
	
	ActorPackerActions = MakeShared<FPCGExActorDataPackerActions>();
	FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(ActorPackerActions.ToSharedRef());
	
	// I know this is cursed
	FSlateStyleSet& AppStyle = const_cast<FSlateStyleSet&>(static_cast<const FSlateStyleSet&>(FAppStyle::Get()));

	Style = MakeShared<FSlateStyleSet>("PCGExStyleSet");
	Style->SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("PCGExtendedToolkit"))->GetBaseDir() / TEXT("Resources") / TEXT("Icons"));

	const FVector2D SizeIcon = FVector2D(16.0f, 16.0f);
	const FVector2D SizePin = FVector2D(22.0f, 22.0f);
	const FVector2D SizeThumbnail = FVector2D(128.0f, 128.0f);

	PCGEX_ADD_ICON(PCGExAssetCollection)
	PCGEX_ADD_ICON(PCGExMeshCollection)
	PCGEX_ADD_ICON(PCGExActorCollection)
	PCGEX_ADD_ICON(PCGExCustomGraphSettings)
	PCGEX_ADD_ICON(PCGExCustomGraphBuilder)
	PCGEX_ADD_ICON(PCGExCustomActorDataPacker)
	PCGEX_ADD_ICON(PCGExBeacon)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Filter)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_CFilter)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_Filters)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Heuristic)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_Heuristics)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Sorting)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_Sortings)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Probe)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_Probes)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Tex)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_Tex)

	PCGEX_ADD_PIN_EXTRA_ICON(IN_Vtx)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Edges)

	PCGEX_ADD_PIN_EXTRA_ICON(IN_Special)

	PCGEX_ADD_PIN_EXTRA_ICON(IN_Partitions)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Partition)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_FilterNode)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_FilterEdges)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_NodeFlag)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_VtxProperty)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Action)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Blend)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Shape)
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Tensor)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Picker)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_FillControl)
	
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Matching)

	FSlateStyleRegistry::RegisterSlateStyle(*Style.Get());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPCGExtendedToolkitEditorModule::RegisterMenuExtensions));

	if (GetDefault<UPCGExGlobalSettings>()->bPersistentDebug)
	{
		FPCGDataVisualizationRegistry& DataVisRegistry = FPCGModule::GetMutablePCGDataVisualizationRegistry();
		DataVisRegistry.RegisterPCGDataVisualization(UPCGSpatialData::StaticClass(), MakeUnique<const IPCGExSpatialDataVisualization>());
	}
}

#undef PCGEX_ADD_ICON

void FPCGExtendedToolkitEditorModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Style->GetStyleSetName());
	Style.Reset();
}

void FPCGExtendedToolkitEditorModule::RegisterMenuExtensions()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	if (UToolMenu* WorldAssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.AssetActionsSubMenu"))
	{
		// Use a dynamic section here because we might have plugins registering at a later time
		FToolMenuSection& Section = WorldAssetMenu->AddDynamicSection(
			"PCGEx", FNewToolMenuDelegate::CreateLambda(
				[this](UToolMenu* ToolMenu)
				{
					if (!GEditor || GEditor->GetPIEWorldContext() || !ToolMenu)
					{
						return;
					}

					if (UContentBrowserAssetContextMenuContext* AssetMenuContext = ToolMenu->Context.FindContext<UContentBrowserAssetContextMenuContext>())
					{
						PCGExEditorMenuUtils::CreateOrUpdatePCGExAssetCollectionsFromMenu(ToolMenu, AssetMenuContext->SelectedAssets);
					}
				}), FToolMenuInsert(NAME_None, EToolMenuInsertType::Default));
	}
}

void FPCGExtendedToolkitEditorModule::UnregisterMenuExtensions()
{
	UToolMenus::UnregisterOwner(this);
}
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitEditorModule, PCGExtendedToolkitEditor)
