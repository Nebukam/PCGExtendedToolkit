// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "PCGExVersion.h"

#if WITH_EDITOR
#include "Styling/SlateStyle.h"
#include "PCGExCoreSettingsCache.h"
#endif

#define PCGEX_ADD_CLASS_ICON(_NAME) \
InStyle->Set("ClassIcon." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), FVector2D(16.0f)));\
InStyle->Set("ClassThumbnail." # _NAME, new FSlateImageBrush(Style->RootToContentDir(TEXT( "" #_NAME), TEXT(".png")), FVector2D(128.0f)));

#define PCGEX_REGISTER_PIN_ICON(_NAME) \
const_cast<FSlateStyleSet&>(static_cast<const FSlateStyleSet&>(FAppStyle::Get()))\
.Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(InStyle->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), FVector2D(22.0f)));\
InStyle->Set("PCGEx.Pin." # _NAME, new FSlateVectorImageBrush(InStyle->RootToContentDir(TEXT( "PCGEx_Pin_" #_NAME), TEXT(".svg")), FVector2D(22.0f)));

#pragma region PCG Registry macros
// Macro swap between 5.6 / 5.7 since type registry didn't exist in 5.6

#if PCGEX_ENGINE_VERSION < 507

#define PCGEX_START_PCG_REGISTRATION

#define PCGEX_REGISTER_DATA_TYPE_INTERNAL(_MODULE, _NAME) \
PCGEX_REGISTER_PIN_ICON(OUT_##_NAME) \
PCGEX_REGISTER_PIN_ICON(IN_##_NAME)

#define PCGEX_REGISTER_DATA_TYPE(_MODULE, _NAME) 

#define PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(_MODULE, _NAME, _NATIVE_COLOR)

#else

#define PCGEX_START_PCG_REGISTRATION FPCGDataTypeRegistry& PCGDataTypeRegistry = FPCGModule::GetMutableDataTypeRegistry();

#define PCGEX_REGISTER_DATA_TYPE_INTERNAL(_MODULE, _NAME) \
PCGEX_REGISTER_PIN_ICON(OUT_##_NAME) \
PCGEX_REGISTER_PIN_ICON(IN_##_NAME)\
PCGDataTypeRegistry.RegisterPinIconsFunction(FPCGExDataTypeInfo##_NAME::AsId(),\
	[&](const FPCGDataTypeIdentifier& InId, const FPCGPinProperties& InProperties, const bool bIsInput) -> TTuple<const FSlateBrush*, const FSlateBrush*>\
	{ \
		if(bIsInput){ return {InStyle->GetBrush(FName("PCGEx.Pin.IN_"#_NAME)), InStyle->GetBrush(FName("PCGEx.Pin.IN_"#_NAME))};} \
		else{ return {InStyle->GetBrush(FName("PCGEx.Pin.OUT_"#_NAME)), InStyle->GetBrush(FName("PCGEx.Pin.OUT_"#_NAME))};} \
	});

#define PCGEX_REGISTER_DATA_TYPE(_MODULE, _NAME) \
PCGEX_REGISTER_DATA_TYPE_INTERNAL(_MODULE, _NAME) \
PCGDataTypeRegistry.RegisterPinColorFunction(FPCGExDataTypeInfo##_NAME::AsId(), \
	[&](const FPCGDataTypeIdentifier&) \
	{ \
		return PCGEX_CORE_SETTINGS.GetColor(FName(#_NAME)); \
	});

#define PCGEX_REGISTER_DATA_TYPE_NATIVE_COLOR(_MODULE, _NAME, _NATIVE_COLOR) \
PCGEX_REGISTER_DATA_TYPE_INTERNAL(_MODULE, _NAME) \
PCGDataTypeRegistry.RegisterPinColorFunction(FPCGExDataTypeInfo##_NAME::AsId(), \
	[&](const FPCGDataTypeIdentifier&) \
	{ \
		return PCGEX_CORE_SETTINGS.GetColorOptIn(FName(#_NAME), _NATIVE_COLOR); \
	});
#endif
#pragma endregion

#define PCGEX_IMPLEMENT_MODULE(_CLASS, _NAME) \
IMPLEMENT_MODULE(_CLASS, _NAME) \
FString _CLASS::GetModuleName() const{ return TEXT(#_NAME); }

#define PCGEX_MODULE_BODY\
	protected:\
	virtual FString GetModuleName() const override;

class FPCGDataTypeRegistry;

class PCGEXCORE_API IPCGExModuleInterface : public IModuleInterface
{
public:
	virtual FString GetModuleName() const { return TEXT(""); }

	static TArray<IPCGExModuleInterface*> RegisteredModules;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

#if WITH_EDITOR
	virtual void RegisterToEditor(const TSharedPtr<FSlateStyleSet>& InStyle);

	void RegisterMenuExtensions();
	void UnregisterMenuExtensions();
#endif
};
