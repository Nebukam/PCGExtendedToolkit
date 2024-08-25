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
	FPCGExMeshCollectionCategory::~FPCGExMeshCollectionCategory()
	{
		Indices.Empty();
		Weights.Empty();
		Order.Empty();
	}

	void FPCGExMeshCollectionCategory::BuildFromIndices()
	{
		const int32 NumEntries = Indices.Num();

		PCGEx::ArrayOfIndices(Order, NumEntries);
		for (int i = 0; i < NumEntries; i++) { Weights[i] = Weights[i] / WeightSum; }

		// TODO : Ugh
		// std::sort(Order.begin(), Order.end(), [&](int A, int B) { return Weights[A] < Weights[B]; });
	}
}

#if WITH_EDITOR
void UPCGExMeshCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	for (FPCGExMeshCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.Descriptor.StaticMesh ? FName(Entry.Descriptor.StaticMesh->GetName()) : NAME_None;
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
		const PCGExMeshCollection::FPCGExMeshCollectionCategory* Category = *CachedCategories.Find(Key);
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

	TArray<PCGExMeshCollection::FPCGExMeshCollectionCategory*> TempCategories;

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

		PCGExMeshCollection::FPCGExMeshCollectionCategory** CategoryPtr = CachedCategories.Find(Entry.Category);
		PCGExMeshCollection::FPCGExMeshCollectionCategory* Category = nullptr;

		if (!CategoryPtr)
		{
			Category = new PCGExMeshCollection::FPCGExMeshCollectionCategory(Entry.Category);
			CachedCategories.Add(Entry.Category, Category);
			TempCategories.Add(Category);
		}
		else { Category = *CategoryPtr; }

		Category->Indices.Add(i);
		Category->Weights.Add(Entry.Weight);
		Category->WeightSum += Entry.Weight;
	}

	for (PCGExMeshCollection::FPCGExMeshCollectionCategory* Cate : TempCategories) { Cate->BuildFromIndices(); }

	const int32 NumEntries = CachedIndices.Num();
	PCGEX_SET_NUM_UNINITIALIZED(CachedWeights, NumEntries)

	PCGEx::ArrayOfIndices(Order, NumEntries);

	for (int i = 0; i < NumEntries; i++) { CachedWeights[i] = CachedWeights[i] / WeightSum; }

	// TODO : Ugh
	// std::sort(Order.begin(), Order.end(), [&](const int A, const int B) { return CachedWeights[A] < CachedWeights[B]; });

	return true;
}
