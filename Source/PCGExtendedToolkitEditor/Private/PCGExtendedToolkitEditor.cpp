// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "AssetRegistry/AssetData.h"
#include "EditorStyleSet.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitEditorModule"

#define PCGEX_ADD_ICON(_NAME) \
Style->Set("ClassIcon." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeIcon));\
Style->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeThumbnail));

#define PCGEX_ADD_PIN_EXTRA_ICON(_NAME) \
AppStyle.Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), SizePin));\
Style->Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), SizePin));

#include "AssetToolsModule.h"
#include "ContentBrowserMenuContexts.h"
#include "IAssetTools.h"
#include "PCGDataVisualizationRegistry.h"
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
MACRO(FillControl, __VA_ARGS__) \
MACRO(Heuristic, __VA_ARGS__) \
MACRO(Probe, __VA_ARGS__) \
MACRO(ClusterState, __VA_ARGS__) \
MACRO(Picker, __VA_ARGS__) \
MACRO(NeighborSampler, __VA_ARGS__) \
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

	PCGEX_FOREACH_CUSTOM_DATA_TYPE(PCGEX_REGISTER_PIN_ICON)

#undef PCGEX_REGISTER_PIN_AND_COLOR

	FSlateStyleRegistry::RegisterSlateStyle(*Style.Get());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPCGExtendedToolkitEditorModule::RegisterMenuExtensions));

	RegisterDataVisualizations();
	RegisterPinColorAndIcons();
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

void FPCGExtendedToolkitEditorModule::RegisterPinColorAndIcons()
{
	FPCGDataTypeRegistry& InRegistry = FPCGModule::GetMutableDataTypeRegistry();

	//InRegistry.RegisterPinColorFunction(FPCGExDataTypeInfo##_NAME::AsId(), [](const FPCGDataTypeIdentifier&) { return GetDefault<UPCGExGlobalSettings>()->PinColor##_COLOR; }); \
	
#define PCGEX_REGISTER_PIN_AND_COLOR(_NAME, _COLOR) \
	InRegistry.RegisterPinColorFunction(FPCGExDataTypeInfo##_NAME::AsId(), [](const FPCGDataTypeIdentifier&) { return FLinearColor::White; }); \
	InRegistry.RegisterPinIconsFunction(FPCGExDataTypeInfo##_NAME::AsId(), [&](const FPCGDataTypeIdentifier& InId, const FPCGPinProperties& InProperties, const bool bIsInput) -> TTuple<const FSlateBrush*, const FSlateBrush*>{ \
		if(bIsInput){ return {Style->GetBrush(FName("PCGEx.Pin.IN_"#_NAME)), Style->GetBrush(FName("PCGEx.Pin.IN_"#_NAME))};}\
		else{ return {Style->GetBrush(FName("PCGEx.Pin.OUT_"#_NAME)), Style->GetBrush(FName("PCGEx.Pin.OUT_"#_NAME))};}});

	PCGEX_FOREACH_CUSTOM_DATA_TYPE(PCGEX_REGISTER_PIN_AND_COLOR, Default)

	/*
	InRegistry.RegisterPinColorFunction(PCGExClusterFilter::GetVtxFiltersCompositeId(), [](const FPCGDataTypeIdentifier&) { return FLinearColor::White; });
	InRegistry.RegisterPinIconsFunction(
		PCGExClusterFilter::GetVtxFiltersCompositeId(),
		[&](const FPCGDataTypeIdentifier& InId, const FPCGPinProperties& InProperties, const bool bIsInput) -> TTuple<const FSlateBrush*, const FSlateBrush*>
		{
			if (bIsInput) { return {Style->GetBrush(FName("PCGEx.Pin.IN_FilterVtx")), Style->GetBrush(FName("PCGEx.Pin.IN_FilterVtx"))}; }
			else { return {Style->GetBrush(FName("PCGEx.Pin.OUT_FilterVtx")), Style->GetBrush(FName("PCGEx.Pin.OUT_FilterVtx"))}; }
		});

	InRegistry.RegisterPinColorFunction(PCGExClusterFilter::GetEdgeFiltersCompositeId(), [](const FPCGDataTypeIdentifier&) { return FLinearColor::White; });
	InRegistry.RegisterPinIconsFunction(
		PCGExClusterFilter::GetEdgeFiltersCompositeId(),
		[&](const FPCGDataTypeIdentifier& InId, const FPCGPinProperties& InProperties, const bool bIsInput) -> TTuple<const FSlateBrush*, const FSlateBrush*>
		{
			if (bIsInput) { return {Style->GetBrush(FName("PCGEx.Pin.IN_FilterEdge")), Style->GetBrush(FName("PCGEx.Pin.IN_FilterEdge"))}; }
			else { return {Style->GetBrush(FName("PCGEx.Pin.OUT_FilterEdge")), Style->GetBrush(FName("PCGEx.Pin.OUT_FilterEdge"))}; }
		});
*/

#undef PCGEX_REGISTER_PIN_AND_COLOR
	/*
	// PinIcons. Just have catch all as most of it will be the same icons.
	InRegistry.RegisterPinIconsFunction(FPCGDataTypeInfo::AsId(), [](const FPCGDataTypeIdentifier& InId, const FPCGPinProperties& InProperties, const bool bIsInput) -> TTuple<const FSlateBrush*, const FSlateBrush*>
	{
		const bool bIsMultiData = InProperties.bAllowMultipleData;
		const bool bIsMultiConnections = InProperties.AllowsMultipleConnections();

		FName ConnectedBrushName = NAME_None;
		FName DisconnectedBrushName = NAME_None;

		if (InId.IsSameType(FPCGDataTypeInfoParam::AsId()))
		{
			ConnectedBrushName = bIsInput ? PCGEditorStyleConstants::Pin_Param_IN_C : PCGEditorStyleConstants::Pin_Param_OUT_C;
			DisconnectedBrushName = bIsInput ? PCGEditorStyleConstants::Pin_Param_IN_DC : PCGEditorStyleConstants::Pin_Param_OUT_DC;
		}
		else if (InId.IsSameType(FPCGDataTypeInfoSpatial::AsId()))
		{
			ConnectedBrushName = bIsInput ? PCGEditorStyleConstants::Pin_Composite_IN_C : PCGEditorStyleConstants::Pin_Composite_OUT_C;
			DisconnectedBrushName = bIsInput ? PCGEditorStyleConstants::Pin_Composite_IN_DC : PCGEditorStyleConstants::Pin_Composite_OUT_DC;
		}
		else if (InProperties.Usage == EPCGPinUsage::DependencyOnly)
		{
			ConnectedBrushName = PCGEditorStyleConstants::Pin_Graph_Dependency_C;
			DisconnectedBrushName = PCGEditorStyleConstants::Pin_Graph_Dependency_DC;
		}
		else
		{
			// Node outputs are always single collection (SC).
			static const FName* PinBrushes[] =
			{
				&PCGEditorStyleConstants::Pin_SD_SC_IN_C,
				&PCGEditorStyleConstants::Pin_SD_SC_IN_DC,
				&PCGEditorStyleConstants::Pin_SD_MC_IN_C,
				&PCGEditorStyleConstants::Pin_SD_MC_IN_DC,
				&PCGEditorStyleConstants::Pin_MD_SC_IN_C,
				&PCGEditorStyleConstants::Pin_MD_SC_IN_DC,
				&PCGEditorStyleConstants::Pin_MD_MC_IN_C,
				&PCGEditorStyleConstants::Pin_MD_MC_IN_DC,
				&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
				&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
				&PCGEditorStyleConstants::Pin_SD_SC_OUT_C,
				&PCGEditorStyleConstants::Pin_SD_SC_OUT_DC,
				&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
				&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC,
				&PCGEditorStyleConstants::Pin_MD_SC_OUT_C,
				&PCGEditorStyleConstants::Pin_MD_SC_OUT_DC
			};

			const int32 ConnectedIndex = (bIsInput ? 0 : 8) + (bIsMultiData ? 4 : 0) + (bIsMultiConnections ? 2 : 0);
			const int32 DisconnectedIndex = ConnectedIndex + 1;

			ConnectedBrushName = *PinBrushes[ConnectedIndex];
			DisconnectedBrushName = *PinBrushes[DisconnectedIndex];
		}

		const FPCGEditorStyle& EditorStyle = FPCGEditorStyle::Get();

		return {EditorStyle.GetBrush(ConnectedBrushName), EditorStyle.GetBrush(DisconnectedBrushName)};
	});
	*/
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
