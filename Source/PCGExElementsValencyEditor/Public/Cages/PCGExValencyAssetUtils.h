// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyCommon.h"

class APCGExValencyCage;
class APCGExValencyCageBase;
class UStaticMeshComponent;

/**
 * Shared utility functions for asset scanning and management.
 * Extracted from duplicated code in APCGExValencyCage and APCGExValencyAssetPalette.
 */
namespace PCGExValencyAssetUtils
{
	/** Detect the asset type from a soft object pointer */
	PCGEXELEMENTSVALENCYEDITOR_API EPCGExValencyAssetType DetectAssetType(const TSoftObjectPtr<UObject>& Asset);

	/** Extract material overrides from a static mesh component (only slots that differ from mesh defaults) */
	PCGEXELEMENTSVALENCYEDITOR_API void ExtractMaterialOverrides(const UStaticMeshComponent* MeshComponent, TArray<FPCGExValencyMaterialOverride>& OutOverrides);

	/** Record a material variant for a mesh asset into a variant map */
	PCGEXELEMENTSVALENCYEDITOR_API void RecordMaterialVariant(
		const FSoftObjectPath& MeshPath,
		const TArray<FPCGExValencyMaterialOverride>& Overrides,
		TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>>& OutVariantsMap);

	/**
	 * Check if scanned assets have changed compared to a previous snapshot.
	 * @param OldScannedAssets Previous scanned assets
	 * @param NewScannedAssets Current scanned assets
	 * @param bPreserveLocalTransforms Whether to compare local transforms
	 * @return True if assets have changed
	 */
	PCGEXELEMENTSVALENCYEDITOR_API bool HaveScannedAssetsChanged(
		const TArray<FPCGExValencyAssetEntry>& OldScannedAssets,
		const TArray<FPCGExValencyAssetEntry>& NewScannedAssets,
		bool bPreserveLocalTransforms);

	/**
	 * Compute preserved local transform based on flags.
	 * @param AssetWorldTransform The asset's world transform
	 * @param OwnerWorldTransform The owning actor's world transform (cage or palette center)
	 * @param bPreserveLocalTransforms Master toggle
	 * @param LocalTransformFlags Bitmask of EPCGExLocalTransformFlags
	 * @return The preserved local transform (Identity if preservation disabled)
	 */
	PCGEXELEMENTSVALENCYEDITOR_API FTransform ComputePreservedLocalTransform(
		const FTransform& AssetWorldTransform,
		const FTransform& OwnerWorldTransform,
		bool bPreserveLocalTransforms,
		uint8 LocalTransformFlags);

	/**
	 * Find all cages in the world that have a given actor in their MirrorSources array.
	 * @param Source The actor to search for in MirrorSources
	 * @param World The world to search in
	 * @param OutCages Array to populate with mirroring cages
	 */
	PCGEXELEMENTSVALENCYEDITOR_API void FindMirroringCages(const AActor* Source, UWorld* World, TArray<APCGExValencyCage*>& OutCages);
}
