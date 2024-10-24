// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitEditor.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitEditorModule"

#define PCGEX_ADD_ICON(_NAME) \
StyleSet->Set("ClassIcon." # _NAME, new FSlateImageBrush(StyleSet->RootToContentDir(TEXT( "" #_NAME ".png")), FVector2D(128.0f, 128.0f)));\
StyleSet->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(StyleSet->RootToContentDir(TEXT( "" #_NAME ".png")), FVector2D(128.0f, 128.0f)));

void FPCGExtendedToolkitEditorModule::StartupModule()
{
	const FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("PCGExtendedToolkit"))->GetBaseDir() / TEXT("Resources") / TEXT("Icons");

	static TSharedPtr<FSlateStyleSet> StyleSet = MakeShareable(new FSlateStyleSet("PCGExStyleSet"));
	StyleSet->SetContentRoot(ContentDir);

	PCGEX_ADD_ICON(PCGExAssetCollection)
	PCGEX_ADD_ICON(PCGExCustomGraphSettings)
	PCGEX_ADD_ICON(PCGExCustomGraphBuilder)
	PCGEX_ADD_ICON(PCGExCustomActorDataPacker)

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

#undef PCGEX_ADD_ICON

void FPCGExtendedToolkitEditorModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle("PCGExStyleSet");
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FPCGExtendedToolkitEditorModule, PCGExtendedToolkitEditor)