// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExAssetCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

namespace PCGExAssetCollection
{
	FCategory::~FCategory()
	{
		Indices.Empty();
		Weights.Empty();
		Order.Empty();
	}

	void FCategory::BuildFromIndices()
	{
		const int32 NumEntries = Indices.Num();

		PCGEx::ArrayOfIndices(Order, NumEntries);
		for (int i = 0; i < NumEntries; i++) { Weights[i] = Weights[i] / WeightSum; }

		Order.Sort([&](const int32 A, const int32 B) { return Weights[A] < Weights[B]; });
	}
}

bool FPCGExAssetCollectionEntry::IsValid()
{
	return false;
}

void FPCGExAssetCollectionEntry::OnSubCollectionLoaded()
{
	// TODO : Rebuild cache
}

void UPCGExAssetCollection::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	RefreshDisplayNames();
#endif
}

void UPCGExAssetCollection::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if WITH_EDITOR
	RefreshDisplayNames();
#endif
}

void UPCGExAssetCollection::PostEditImport()
{
	Super::PostEditImport();
#if WITH_EDITOR
	RefreshDisplayNames();
#endif
}

#if WITH_EDITOR
void UPCGExAssetCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshDisplayNames();
}

void UPCGExAssetCollection::RefreshDisplayNames()
{	
	bCacheDirty = true;
}
#endif

void UPCGExAssetCollection::BeginDestroy()
{
	Super::BeginDestroy();

	CachedIndices.Empty();
	CachedWeights.Empty();
	Order.Empty();

	ClearCategories();
}

void UPCGExAssetCollection::ClearCategories()
{
	TArray<FName> Keys;
	CachedCategories.GetKeys(Keys);

	for (FName Key : Keys)
	{
		const PCGExAssetCollection::FCategory* Category = *CachedCategories.Find(Key);
		PCGEX_DELETE(Category)
	}

	CachedCategories.Empty();
}
