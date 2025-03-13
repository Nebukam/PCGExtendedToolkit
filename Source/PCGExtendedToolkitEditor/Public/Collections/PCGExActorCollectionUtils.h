// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Collections/PCGExActorCollection.h"
#include "Engine/World.h"
#include "AssetRegistry/AssetData.h"

class UPackage;

namespace PCGExActorCollectionUtils
{
	void CreateCollectionFrom(const TArray<FAssetData>& SelectedAssets);
	void UpdateCollectionsFrom(
		const TArray<TObjectPtr<UPCGExActorCollection>>& SelectedCollections,
		const TArray<FAssetData>& SelectedAssets,
		bool bIsNewCollection = false);
};
