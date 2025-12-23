// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExCollectionEditorUtils.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Core/PCGExAssetCollection.h"

namespace PCGExCollectionEditorUtils
{
#define PCGEX_IF_TYPE(_NAME, _BODY) { if (UPCGEx##_NAME##Collection* Collection = Cast<UPCGEx##_NAME##Collection>(InCollection)) { _BODY; return; }}
#define PCGEX_PER_COLLECTION(_BODY)	PCGEX_FOREACH_COLLECTION_TYPE(PCGEX_IF_TYPE, _BODY)

	void AddBrowserSelection(UPCGExAssetCollection* InCollection)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> SelectedAssets;
		ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

		if (SelectedAssets.IsEmpty()) { return; }

		InCollection->EDITOR_AddBrowserSelectionTyped(SelectedAssets);
	}

	void SortByWeightAscending(UPCGExAssetCollection* InCollection)
	{
		InCollection->Sort([&](const FPCGExAssetCollectionEntry* A, const FPCGExAssetCollectionEntry* B) { return A->Weight < B->Weight; });
	}

	void SortByWeightDescending(UPCGExAssetCollection* InCollection)
	{
		InCollection->Sort([&](const FPCGExAssetCollectionEntry* A, const FPCGExAssetCollectionEntry* B) { return A->Weight > B->Weight; });
	}

	void SetWeightIndex(UPCGExAssetCollection* InCollection)
	{
		InCollection->ForEachEntry([&](FPCGExAssetCollectionEntry* Entry, int32 i) { Entry->Weight = i + 1; });
	}

	void PadWeight(UPCGExAssetCollection* InCollection)
	{
		InCollection->ForEachEntry([&](FPCGExAssetCollectionEntry* Entry, int32 i) { Entry->Weight += 1; });
	}

	void MultWeight(UPCGExAssetCollection* InCollection, int32 Mult)
	{
		InCollection->ForEachEntry([&](FPCGExAssetCollectionEntry* Entry, int32 i) { Entry->Weight *= Mult; });
	}

	void WeightOne(UPCGExAssetCollection* InCollection)
	{
		InCollection->ForEachEntry([&](FPCGExAssetCollectionEntry* Entry, int32 i) { Entry->Weight = 100; });
	}

	void WeightRandom(UPCGExAssetCollection* InCollection)
	{
		FRandomStream RandomSource(FMath::Rand());
		const int32 NumEntries = InCollection->NumEntries();
		InCollection->ForEachEntry(
			[&](FPCGExAssetCollectionEntry* Entry, int32 i)
			{
				Entry->Weight = RandomSource.RandRange(1, NumEntries * 100);
			});
	}

	void NormalizedWeightToSum(UPCGExAssetCollection* InCollection)
	{
		double Sum = 0;

		InCollection->ForEachEntry([&](const FPCGExAssetCollectionEntry* Entry, int32 i) { Sum += Entry->Weight; });
		InCollection->ForEachEntry([&](FPCGExAssetCollectionEntry* Entry, int32 i)
		{
			int32& W = Entry->Weight;

			if (W <= 0)
			{
				W = 0;
				return;
			}

			const double Weight = (static_cast<double>(W) / Sum) * 100;
			W = Weight;
		});
	}
}
