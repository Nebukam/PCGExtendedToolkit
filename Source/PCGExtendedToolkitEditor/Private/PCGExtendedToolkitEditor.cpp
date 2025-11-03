// Copyright 2025 Timothé Lapetite and contributors
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
#include "PCGEditorSettings.h"
#include "PCGExEditorMenuUtils.h"
#include "PCGExGlobalSettings.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "Actions/PCGExActionFactoryProvider.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Collections/PCGExActorCollectionActions.h"
#include "Collections/PCGExMeshCollectionActions.h"
#include "Collections/PCGExActorDataPackerActions.h"
#include "Data/PCGSpatialData.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"
#include "DataViz/PCGExSpatialDataVisualization.h"
#include "Details/PCGExDetailsCustomization.h"
#include "Graph/Data/PCGExClusterData.h"
#include "Graph/Edges/Properties/PCGExVtxPropertyFactoryProvider.h"
#include "Graph/Filters/PCGExClusterFilter.h"
#include "Graph/FloodFill/FillControls/PCGExFillControlsFactoryProvider.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristicsFactoryProvider.h"
#include "Graph/Probes/PCGExProbeFactoryProvider.h"
#include "Graph/States/PCGExClusterStates.h"
#include "Misc/PCGExModularPartitionByValues.h"
#include "Misc/Pickers/PCGExPickerFactoryProvider.h"
#include "Sampling/PCGExTexParamFactoryProvider.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"
#include "Shapes/PCGExShapeBuilderFactoryProvider.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitEditorModule"

#define PCGEX_ADD_ICON(_NAME) \
Style->Set("ClassIcon." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeIcon));\
Style->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeThumbnail));

#define PCGEX_ADD_PIN_EXTRA_ICON(_NAME) \
AppStyle.Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), SizePin));\
Style->Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), SizePin));

#define PCGEX_FOREACH_CUSTOM_DATA_TYPE(MACRO, ...)\
MACRO(Action, __VA_ARGS__) \
MACRO(BlendOp, __VA_ARGS__) \
MACRO(MatchRule, __VA_ARGS__) \
MACRO(Filter, __VA_ARGS__) \
MACRO(FilterPoint, __VA_ARGS__) \
MACRO(FilterCollection, __VA_ARGS__) \
MACRO(FilterCluster, __VA_ARGS__) \
MACRO(FilterVtx, __VA_ARGS__) \
MACRO(FilterEdge, __VA_ARGS__) \
MACRO(VtxProperty, __VA_ARGS__) \
MACRO(NeighborSampler, __VA_ARGS__) \
MACRO(FillControl, __VA_ARGS__) \
MACRO(Heuristics, __VA_ARGS__) \
MACRO(Probe, __VA_ARGS__) \
MACRO(ClusterState, __VA_ARGS__) \
MACRO(Picker, __VA_ARGS__) \
MACRO(TexParam, __VA_ARGS__) \
MACRO(Shape, __VA_ARGS__) \
MACRO(Tensor, __VA_ARGS__) \
MACRO(SortRule, __VA_ARGS__) \
MACRO(PartitionRule, __VA_ARGS__) \
MACRO(Vtx, __VA_ARGS__) \
MACRO(Edges, __VA_ARGS__)

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

#define PCGEX_REGISTER_PIN_ICON(_NAME, ...) \
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_##_NAME) \
	PCGEX_ADD_PIN_EXTRA_ICON(IN_##_NAME)

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Special)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_Special)
	
	PCGEX_ADD_PIN_EXTRA_ICON(OUT_RecursionTracker)
	PCGEX_ADD_PIN_EXTRA_ICON(IN_RecursionTracker)

	PCGEX_FOREACH_CUSTOM_DATA_TYPE(PCGEX_REGISTER_PIN_ICON)

#undef PCGEX_REGISTER_PIN_AND_COLOR

	FSlateStyleRegistry::RegisterSlateStyle(*Style.Get());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPCGExtendedToolkitEditorModule::RegisterMenuExtensions));
	PCGExDetailsCustomization::RegisterDetailsCustomization();
	
	RegisterDataVisualizations();
}

#undef PCGEX_ADD_ICON

void FPCGExtendedToolkitEditorModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Style->GetStyleSetName());
	Style.Reset();
}

void FPCGExtendedToolkitEditorModule::RegisterDataVisualizations()
{
	if (GetDefault<UPCGExGlobalSettings>()->bPersistentDebug)
	{
		FPCGDataVisualizationRegistry& DataVisRegistry = FPCGModule::GetMutablePCGDataVisualizationRegistry();
		DataVisRegistry.RegisterPCGDataVisualization(UPCGSpatialData::StaticClass(), MakeUnique<const IPCGExSpatialDataVisualization>());
	}
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
#undef PCGEX_FOREACH_CUSTOM_DATA_TYPE
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitEditorModule, PCGExtendedToolkitEditor)
