// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitEditorModule"

#define PCGEX_ADD_ICON(_NAME) \
Style->Set("ClassIcon." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeIcon));\
Style->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), SizeThumbnail));

#define PCGEX_ADD_PIN_EXTRA_ICON(_NAME) \
AppStyle.Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGExPin_" #_NAME), TEXT(".svg")), SizePin));\
Style->Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(Style->RootToContentDir(TEXT( "PCGExPin_" #_NAME), TEXT(".svg")), SizePin));

#include "PCGGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"

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

	PCGEX_ADD_PIN_EXTRA_ICON(OUT_Filter)
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

	FSlateStyleRegistry::RegisterSlateStyle(*Style.Get());
}

#undef PCGEX_ADD_ICON

void FPCGExtendedToolkitEditorModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Style->GetStyleSetName());
	Style.Reset();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitEditorModule, PCGExtendedToolkitEditor)
