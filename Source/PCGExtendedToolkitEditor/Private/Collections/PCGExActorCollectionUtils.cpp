// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExActorCollectionUtils.h"

#include "PCGExtendedToolkitEditor.h"
#include "FileHelpers.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"

void PCGExActorCollectionUtils::CreateCollectionFrom(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.IsEmpty()) { return; }

	// TODO
}

void PCGExActorCollectionUtils::UpdateCollectionsFrom(
	const TArray<TObjectPtr<UPCGExActorCollection>>& SelectedCollections,
	const TArray<FAssetData>& SelectedAssets,
	bool bIsNewCollection)
{
}
