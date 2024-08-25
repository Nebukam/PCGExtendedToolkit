// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "MeshSelectors/PCGExMeshCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"

void FPCGExMeshCollectionEntry::LoadSubCollection()
{
	if (!SubCollection.ToSoftObjectPath().IsValid()) { return; }
	SubCollectionPtr = SubCollection.LoadSynchronous();
	if (SubCollection) { SubCollection->RebuildCachedData(); }
}

namespace PCGExMeshCollection
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

void UPCGExMeshCollection::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	RefreshDisplayNames();
#endif
}

void UPCGExMeshCollection::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if WITH_EDITOR
	RefreshDisplayNames();
#endif
}

void UPCGExMeshCollection::PostEditImport()
{
	Super::PostEditImport();
#if WITH_EDITOR
	RefreshDisplayNames();
#endif
}

#if WITH_EDITOR
void UPCGExMeshCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshDisplayNames();
}

void UPCGExMeshCollection::RefreshDisplayNames()
{
	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bSubCollection ? FName(TEXT("+ ") + Entry.SubCollection.GetAssetName()) : FName(Entry.Descriptor.StaticMesh.GetAssetName());
	}

	bCacheDirty = true;
}
#endif

void UPCGExMeshCollection::BeginDestroy()
{
	Super::BeginDestroy();

	CachedIndices.Empty();
	CachedWeights.Empty();
	Order.Empty();

	ClearCategories();
}

void UPCGExMeshCollection::ClearCategories()
{
	TArray<FName> Keys;
	CachedCategories.GetKeys(Keys);

	for (FName Key : Keys)
	{
		const PCGExMeshCollection::FCategory* Category = *CachedCategories.Find(Key);
		PCGEX_DELETE(Category)
	}

	CachedCategories.Empty();
}

bool UPCGExMeshCollection::RebuildCachedData()
{
	if (!bCacheDirty) { return false; }

	bCacheDirty = false;
	ClearCategories();

	CachedIndices.Empty();
	CachedIndices.Reserve(Entries.Num());

	CachedWeights.Empty();
	CachedWeights.Reserve(Entries.Num());

	TArray<PCGExMeshCollection::FCategory*> TempCategories;

	double WeightSum = 0;

	for (int i = 0; i < Entries.Num(); i++)
	{
		FPCGExMeshCollectionEntry& Entry = Entries[i];
		if (Entry.bSubCollection)
		{
			Entry.LoadSubCollection();
			if (!Entry.SubCollectionPtr) { continue; }
		}
		else if (!Entry.Descriptor.StaticMesh) { continue; }

		CachedIndices.Add(i);

		CachedWeights.Add(Entry.Weight);
		WeightSum += Entry.Weight;

		if (Entry.Category.IsNone()) { continue; }

		PCGExMeshCollection::FCategory** CategoryPtr = CachedCategories.Find(Entry.Category);
		PCGExMeshCollection::FCategory* Category = nullptr;

		if (!CategoryPtr)
		{
			Category = new PCGExMeshCollection::FCategory(Entry.Category);
			CachedCategories.Add(Entry.Category, Category);
			TempCategories.Add(Category);
		}
		else { Category = *CategoryPtr; }

		Category->Indices.Add(i);
		Category->Weights.Add(Entry.Weight);
		Category->WeightSum += Entry.Weight;
	}

	for (PCGExMeshCollection::FCategory* Cate : TempCategories) { Cate->BuildFromIndices(); }

	const int32 NumEntries = CachedIndices.Num();
	PCGEX_SET_NUM_UNINITIALIZED(CachedWeights, NumEntries)

	PCGEx::ArrayOfIndices(Order, NumEntries);

	for (int i = 0; i < NumEntries; i++)
	{
		CachedWeights[i] = i == 0 ? CachedWeights[i] / WeightSum : CachedWeights[i - 1] + CachedWeights[i] / WeightSum;
	}

	Order.Sort([&](const int32 A, const int32 B) { return CachedWeights[A] < CachedWeights[B]; });

	return true;
}
