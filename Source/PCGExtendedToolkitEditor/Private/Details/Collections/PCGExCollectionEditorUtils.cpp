// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Collections/PCGExCollectionEditorUtils.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Collections/PCGExCollectionHelpers.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Collections/PCGExPCGDataAssetCollection.h"

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
		PCGEX_PER_COLLECTION(SortByWeightAscendingTpl(Collection->Entries))
	}

	void SortByWeightDescending(UPCGExAssetCollection* InCollection)
	{
		PCGEX_PER_COLLECTION(SortByWeightDescendingTpl(Collection->Entries))
	}

	void SetWeightIndex(UPCGExAssetCollection* InCollection)
	{
		PCGEX_PER_COLLECTION(SetWeightIndexTpl(Collection->Entries))
	}

	void PadWeight(UPCGExAssetCollection* InCollection)
	{
		PCGEX_PER_COLLECTION(PadWeightTpl(Collection->Entries))
	}

	void MultWeight(UPCGExAssetCollection* InCollection, int32 Mult)
	{
		PCGEX_PER_COLLECTION(MultWeightTpl(Collection->Entries, Mult))
	}

	void WeightOne(UPCGExAssetCollection* InCollection)
	{
		PCGEX_PER_COLLECTION(WeightOneTpl(Collection->Entries))
	}

	void WeightRandom(UPCGExAssetCollection* InCollection)
	{
		PCGEX_PER_COLLECTION(WeightRandomTpl(Collection->Entries))
	}

	void NormalizedWeightToSum(UPCGExAssetCollection* InCollection)
	{
		PCGEX_PER_COLLECTION(NormalizedWeightToSumTpl(Collection->Entries))
	}
}
