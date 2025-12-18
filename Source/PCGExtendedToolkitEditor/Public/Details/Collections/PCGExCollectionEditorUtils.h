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

	template <typename T>
	void SortByWeightAscendingTpl(TArray<T>& Entries)
	{
		Entries.Sort([](const T& A, const T& B) { return A.Weight < B.Weight; });
	}

	/** Sort collection by weights in descending order. */
	void SortByWeightDescending(UPCGExAssetCollection* InCollection);

	template <typename T>
	void SortByWeightDescendingTpl(TArray<T>& Entries)
	{
		Entries.Sort([](const T& A, const T& B) { return A.Weight > B.Weight; });
	}

	/**Sort collection by weights in descending order. */
	void SetWeightIndex(UPCGExAssetCollection* InCollection);

	template <typename T>
	void SetWeightIndexTpl(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = i + 1; }
	}

	/** Add 1 to all weights so it's easier to weight down some assets */
	void PadWeight(UPCGExAssetCollection* InCollection);

	template <typename T>
	void PadWeightTpl(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight += 1; }
	}

	/** Multiplies all weights by 2 */
	void MultWeight(UPCGExAssetCollection* InCollection, int32 Mult);

	template <typename T>
	void MultWeightTpl(TArray<T>& Entries, int32 Mult)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight *= Mult; }
	}

	/** Reset all weights to 100 */
	void WeightOne(UPCGExAssetCollection* InCollection);

	template <typename T>
	void WeightOneTpl(TArray<T>& Entries)
	{
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = 100; }
	}

	/** Assign random weights to items */
	void WeightRandom(UPCGExAssetCollection* InCollection);

	template <typename T>
	void WeightRandomTpl(TArray<T>& Entries)
	{
		FRandomStream RandomSource(FMath::Rand());
		for (int i = 0; i < Entries.Num(); i++) { Entries[i].Weight = RandomSource.RandRange(1, Entries.Num() * 100); }
	}

	/** Normalize weight sum to 100 */
	void NormalizedWeightToSum(UPCGExAssetCollection* InCollection);

	template <typename T>
	void NormalizedWeightToSumTpl(TArray<T>& Entries)
	{
		double Sum = 0;
		for (int i = 0; i < Entries.Num(); i++) { Sum += Entries[i].Weight; }
		for (int i = 0; i < Entries.Num(); i++)
		{
			int32& W = Entries[i].Weight;
			if (W <= 0)
			{
				W = 0;
				continue;
			}
			const double Weight = (static_cast<double>(Entries[i].Weight) / Sum) * 100;
			W = Weight;
		}
	}

#pragma endregion
}
