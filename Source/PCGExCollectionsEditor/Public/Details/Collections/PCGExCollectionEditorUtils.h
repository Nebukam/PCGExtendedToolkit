// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

class UPCGExAssetCollection;
class UPackage;

namespace PCGExCollectionEditorUtils
{
	/** Add Content Browser selection to this collection. */
	void AddBrowserSelection(UPCGExAssetCollection* InCollection);

#pragma region Tools

	/** Sort collection by weights in ascending order. */
	void SortByWeightAscending(UPCGExAssetCollection* InCollection);

	/** Sort collection by weights in descending order. */
	void SortByWeightDescending(UPCGExAssetCollection* InCollection);

	/**Sort collection by weights in descending order. */
	void SetWeightIndex(UPCGExAssetCollection* InCollection);

	/** Add 1 to all weights so it's easier to weight down some assets */
	void PadWeight(UPCGExAssetCollection* InCollection);

	/** Multiplies all weights by 2 */
	void MultWeight(UPCGExAssetCollection* InCollection, int32 Mult);

	/** Reset all weights to 100 */
	void WeightOne(UPCGExAssetCollection* InCollection);

	/** Assign random weights to items */
	void WeightRandom(UPCGExAssetCollection* InCollection);

	/** Normalize weight sum to 100 */
	void NormalizedWeightToSum(UPCGExAssetCollection* InCollection);

#pragma endregion
}
