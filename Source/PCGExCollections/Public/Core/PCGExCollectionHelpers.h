// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "Core/PCGExContext.h"
#include "PCGParamData.h"
#include "Details/PCGExStagingDetails.h"

/**
 * Collection Helper Functions
 * 
 * Simplified API for working with asset collections.
 * Most functionality is now in the base UPCGExAssetCollection class.
 * These helpers provide convenience functions and attribute set building.
 */

namespace PCGExCollectionHelpers
{
	/**
	 * Build a collection from an attribute set
	 * @param InCollection Target collection to populate
	 * @param InContext PCG context
	 * @param InAttributeSet Attribute set containing asset paths
	 * @param Details Configuration for how to map attributes to entries
	 * @param bBuildStaging Whether to rebuild staging data after building
	 * @return true if successful
	 */
	PCGEXCOLLECTIONS_API
	bool BuildFromAttributeSet(
		UPCGExAssetCollection* InCollection,
		FPCGExContext* InContext,
		const UPCGParamData* InAttributeSet,
		const FPCGExAssetAttributeSetDetails& Details,
		bool bBuildStaging = false);

	/**
	 * Build a collection from an attribute set on a specific input pin
	 */
	PCGEXCOLLECTIONS_API
	bool BuildFromAttributeSet(
		UPCGExAssetCollection* InCollection,
		FPCGExContext* InContext,
		FName InputPin,
		const FPCGExAssetAttributeSetDetails& Details,
		bool bBuildStaging);

	/**
	 * Accumulate tags from entry and potentially its subcollection hierarchy
	 * @param Entry The entry to get tags from
	 * @param TagInheritance Bitmask of EPCGExAssetTagInheritance flags
	 * @param OutTags Set to append tags to
	 */
	PCGEXCOLLECTIONS_API
	void AccumulateTags(
		const FPCGExAssetCollectionEntry* Entry,
		uint8 TagInheritance,
		TSet<FName>& OutTags);

	/**
	 * Get all asset paths from a collection recursively
	 * @param Collection Source collection
	 * @param OutPaths Set to append paths to
	 * @param bRecursive Whether to include subcollection assets
	 */
	PCGEXCOLLECTIONS_API
	void GetAllAssetPaths(
		const UPCGExAssetCollection* Collection,
		TSet<FSoftObjectPath>& OutPaths,
		bool bRecursive = true);

	/**
	 * Check if a collection or any of its subcollections contain an asset
	 * @param Collection Collection to search
	 * @param AssetPath Path to look for
	 * @return true if found
	 */
	PCGEXCOLLECTIONS_API
	bool ContainsAsset(
		const UPCGExAssetCollection* Collection,
		const FSoftObjectPath& AssetPath);

	/**
	 * Count total entries including subcollections
	 * @param Collection Collection to count
	 * @return Total entry count
	 */
	PCGEXCOLLECTIONS_API
	int32 CountTotalEntries(const UPCGExAssetCollection* Collection);

	/**
	 * Flatten a hierarchical collection into a single level
	 * Creates copies of entries from subcollections with inherited properties
	 * @param Source Source collection
	 * @param Target Target collection (must be same type as source)
	 * @return true if successful
	 */
	PCGEXCOLLECTIONS_API
	bool FlattenCollection(
		const UPCGExAssetCollection* Source,
		UPCGExAssetCollection* Target);
}
