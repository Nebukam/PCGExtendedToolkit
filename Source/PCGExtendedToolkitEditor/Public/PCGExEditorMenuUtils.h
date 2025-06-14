// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "ToolMenus.h"
#include "AssetRegistry/AssetData.h"

namespace PCGExEditorMenuUtils
{
	PCGEXTENDEDTOOLKITEDITOR_API FToolMenuSection& CreatePCGExSection(UToolMenu* Menu);
	PCGEXTENDEDTOOLKITEDITOR_API void CreateOrUpdatePCGExAssetCollectionsFromMenu(UToolMenu* Menu, TArray<FAssetData>& Assets);

	bool DoesAssetInheritFromAActor(const FAssetData& AssetData);
}
