// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkit.h"
#include "PCGExVersion.h"

#if WITH_EDITOR
#include "PCGExCoreSettingsCache.h"
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitModule"

void FPCGExtendedToolkitModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// We need this because the registry holds a reference to the collection ::StaticClass
	// and it cannot be access during initialization so we defer it here.


#pragma region Push Pins

#if WITH_EDITOR

	int32 PinIndex = -1;

#pragma region OUT

	PCGEX_EMPLACE_PIN_OUT(OUT_Vtx, "Point collection formatted for use as cluster vtx.");
	PCGEX_MAP_PIN_OUT("Vtx")
	PCGEX_MAP_PIN_OUT("Unmatched Vtx")

	PCGEX_EMPLACE_PIN_OUT(OUT_Edges, "Point collection formatted for use as cluster edges.");
	PCGEX_MAP_PIN_OUT("Edges")
	PCGEX_MAP_PIN_OUT("Unmatched Edges")

#if PCGEX_ENGINE_VERSION < 506
	// Register pins as extra icons on 5.6
#endif

#pragma endregion

#pragma region IN

	PCGEX_EMPLACE_PIN_IN(IN_Vtx, "Point collection formatted for use as cluster vtx.");
	PCGEX_MAP_PIN_IN("Vtx")

	PCGEX_EMPLACE_PIN_IN(IN_Edges, "Point collection formatted for use as cluster edges.");
	PCGEX_MAP_PIN_IN("Edges")

#if PCGEX_ENGINE_VERSION < 506
	// Register pins as extra icons on 5.6
#endif

	PCGEX_EMPLACE_PIN_IN(IN_Special, "Attribute set whose values will be used to override a specific internal module.");
	PCGEX_MAP_PIN_IN("Overrides : Blending")
	PCGEX_MAP_PIN_IN("Overrides : Refinement")
	PCGEX_MAP_PIN_IN("Overrides : Graph Builder")
	PCGEX_MAP_PIN_IN("Overrides : Tangents")
	PCGEX_MAP_PIN_IN("Overrides : Start Tangents")
	PCGEX_MAP_PIN_IN("Overrides : End Tangents")
	PCGEX_MAP_PIN_IN("Overrides : Goal Picker")
	PCGEX_MAP_PIN_IN("Overrides : Search")
	PCGEX_MAP_PIN_IN("Overrides : Orient")
	PCGEX_MAP_PIN_IN("Overrides : Smoothing")
	PCGEX_MAP_PIN_IN("Overrides : Packer")

#pragma endregion

#endif

#pragma endregion
}

void FPCGExtendedToolkitModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitModule, PCGExtendedToolkit)
