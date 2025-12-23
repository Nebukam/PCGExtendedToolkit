// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "PCGExCoreSettingsCache.h"
#endif

#define PCGEX_ADD_CLASS_ICON(_NAME) \
InStyle->Set("ClassIcon." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), FVector2D(16.0f)));\
InStyle->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), FVector2D(128.0f)));

#define PCGEX_REGISTER_PIN_ICON(_NAME) \
const_cast<FSlateStyleSet&>(static_cast<const FSlateStyleSet&>(FAppStyle::Get()))\
.Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(InStyle->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), FVector2D(22.0f)));\
InStyle->Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(InStyle->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), FVector2D(22.0f)));

#define PCGEX_REGISTER_DATA_TYPE(_MODULE, _NAME) \
PCGEX_REGISTER_PIN_ICON(OUT_##_NAME) \
PCGEX_REGISTER_PIN_ICON(IN_##_NAME)\
InRegistry.RegisterPinColorFunction(FPCGExDataTypeInfo##_NAME::AsId(), [&](const FPCGDataTypeIdentifier&) { return PCGEX_NODE_COLOR_NAME(_NAME); }); \
InRegistry.RegisterPinIconsFunction(FPCGExDataTypeInfo##_NAME::AsId(), [&](const FPCGDataTypeIdentifier& InId, const FPCGPinProperties& InProperties, const bool bIsInput) -> TTuple<const FSlateBrush*, const FSlateBrush*>{ \
if(bIsInput){ return {InStyle->GetBrush(FName("PCGEx.Pin.IN_"#_NAME)), InStyle->GetBrush(FName("PCGEx.Pin.IN_"#_NAME))};}\
else{ return {InStyle->GetBrush(FName("PCGEx.Pin.OUT_"#_NAME)), InStyle->GetBrush(FName("PCGEx.Pin.OUT_"#_NAME))};}});

#define PCGEX_IMPLEMENT_MODULE(_CLASS, _NAME) \
IMPLEMENT_MODULE(_CLASS, _NAME) \
FString _CLASS::GetModuleName() const{ return TEXT(#_NAME); }

#define PCGEX_MODULE_BODY\
	protected:\
	virtual FString GetModuleName() const override;

class FPCGDataTypeRegistry;

class PCGEXCORE_API IPCGExModuleInterface : public IModuleInterface
{
protected:
	virtual FString GetModuleName() const { return TEXT(""); }
	
public:
	// Static registry
	static TArray<IPCGExModuleInterface*> RegisteredModules;


	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR
	virtual void RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle, FPCGDataTypeRegistry& InRegistry);

	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
#endif
};
