// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Collections/PCGExMeshCollection.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetData.h"

class UPackage;

namespace PCGExMeshCollectionUtils
{
	void CreateCollectionFrom(const TArray<FAssetData>& SelectedAssets);
	void UpdateCollectionsFrom(
		const TArray<TObjectPtr<UPCGExMeshCollection>>& SelectedCollections,
		const TArray<FAssetData>& SelectedAssets,
		bool bIsNewCollection = false);
};
