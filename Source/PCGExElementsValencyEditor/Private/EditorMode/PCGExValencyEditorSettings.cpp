// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyEditorSettings.h"

#include "Materials/MaterialInterface.h"

UPCGExValencyEditorSettings::UPCGExValencyEditorSettings()
{
	// Default values are set in the header via UPROPERTY initializers
}

const UPCGExValencyEditorSettings* UPCGExValencyEditorSettings::Get()
{
	return GetDefault<UPCGExValencyEditorSettings>();
}

UMaterialInterface* UPCGExValencyEditorSettings::GetGhostMaterial() const
{
	// Try to load the configured ghost material
	if (!GhostMaterial.IsNull())
	{
		if (UMaterialInterface* LoadedMaterial = GhostMaterial.LoadSynchronous())
		{
			return LoadedMaterial;
		}
	}

	// Fallback to engine's world grid material
	static UMaterialInterface* DefaultGridMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));

	return DefaultGridMaterial;
}
