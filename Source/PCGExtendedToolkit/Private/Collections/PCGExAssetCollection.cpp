// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExAssetCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace PCGExAssetCollection
{
	void FCategory::RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry)
	{
		Entries.Add(InEntry);

		Indices.Add(Index);

		Weights.Add(InEntry->Weight);
	}

	void FCategory::Compile()
	{
		Shrink();

		const int32 NumEntries = Indices.Num();
		PCGEx::ArrayOfIndices(Order, NumEntries);

		Order.Sort([&](const int32 A, const int32 B) { return Weights[A] < Weights[B]; });
		Weights.Sort([](const int32 A, const int32 B) { return A < B; });

		WeightSum = 0;
		for (int32 i = 0; i < NumEntries; i++)
		{
			WeightSum += Weights[i];
			Weights[i] = WeightSum;
		}
	}
}

bool FPCGExAssetCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection)
	{
		if (!BaseSubCollectionPtr) { return false; }
		BaseSubCollectionPtr->LoadCache();
	}
	return true;
}

void FPCGExAssetCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection) { Staging.Bounds = FBox(ForceInitToZero); }
}

void FPCGExAssetCollectionEntry::OnSubCollectionLoaded()
{
}

namespace PCGExAssetCollection
{
	void FCache::RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry)
	{
		// Register to main category
		Main->RegisterEntry(Index, InEntry);

		// Register to sub categories
		if (const TSharedPtr<FCategory>* CategoryPtr = Categories.Find(InEntry->Category); !CategoryPtr)
		{
			const TSharedPtr<FCategory> Category = MakeShared<FCategory>(InEntry->Category);
			Categories.Add(InEntry->Category, Category);
			Category->RegisterEntry(Index, InEntry);
		}
		else
		{
			(*CategoryPtr)->RegisterEntry(Index, InEntry);
		}
	}

	void FCache::Compile()
	{
		Main->Compile();
		for (const TPair<FName, TSharedPtr<FCategory>>& Pair : Categories) { Pair.Value->Compile(); }
	}
}

PCGExAssetCollection::FCache* UPCGExAssetCollection::LoadCache()
{
	{
		FReadScopeLock ReadScopeLock(CacheLock);
		if (bCacheNeedsRebuild) { InvalidateCache(); }
		if (Cache) { return Cache.Get(); }
	}

	BuildCache();
	return Cache.Get();
}

void UPCGExAssetCollection::InvalidateCache()
{
	Cache.Reset();
	bCacheNeedsRebuild = true;
}

void UPCGExAssetCollection::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	EDITOR_RefreshDisplayNames();
	EDITOR_SetDirty();
#endif
}

void UPCGExAssetCollection::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
#if WITH_EDITOR
	EDITOR_RefreshDisplayNames();
	EDITOR_SetDirty();
#endif
}

void UPCGExAssetCollection::PostEditImport()
{
	Super::PostEditImport();
#if WITH_EDITOR
	EDITOR_RefreshDisplayNames();
	EDITOR_SetDirty();
#endif
}

void UPCGExAssetCollection::RebuildStagingData(const bool bRecursive)
{
	InvalidateCache();
}

#if WITH_EDITOR
void UPCGExAssetCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	EDITOR_RefreshDisplayNames();
	EDITOR_SetDirty();

	if (bAutoRebuildStaging) { EDITOR_RebuildStagingData(); }
}

void UPCGExAssetCollection::EDITOR_RefreshDisplayNames()
{
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData()
{
	Modify(true);
	EDITOR_SanitizeAndRebuildStagingData(false);
	MarkPackageDirty();
	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData_Recursive()
{
	Modify(true);
	EDITOR_SanitizeAndRebuildStagingData(true);
	MarkPackageDirty();
	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData_Project()
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		if (UPCGExAssetCollection* Collection = Cast<UPCGExAssetCollection>(AssetData.GetAsset())) { Collection->EDITOR_RebuildStagingData(); }
	}
}

#define PCGEX_ASSET_COLLECTION_UTIL_CALL(_BODY) \
Modify(true); \
_BODY \
FPropertyChangedEvent EmptyEvent(nullptr); \
PostEditChangeProperty(EmptyEvent); \
MarkPackageDirty();

void UPCGExAssetCollection::EDITOR_SortByWeightAscending() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_SortByWeightAscendingTyped();) }

void UPCGExAssetCollection::EDITOR_SortByWeightDescending() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_SortByWeightDescendingTyped();) }

void UPCGExAssetCollection::EDITOR_SetWeightIndex() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_SetWeightIndexTyped();) }

void UPCGExAssetCollection::EDITOR_PadWeight() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_PadWeightTyped();) }

void UPCGExAssetCollection::EDITOR_WeightOne() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_WeightOneTyped();) }

void UPCGExAssetCollection::EDITOR_WeightRandom() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_WeightRandomTyped();) }

void UPCGExAssetCollection::EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive)
{
}
#endif


void UPCGExAssetCollection::BeginDestroy()
{
	InvalidateCache();
	Super::BeginDestroy();
}

void UPCGExAssetCollection::BuildCache()
{
	bCacheNeedsRebuild = false;
	/* per-class implementation, forwards Entries to protected method */
}

void UPCGExAssetCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const
{
}


bool FPCGExRoamingAssetCollectionDetails::Validate(FPCGExContext* InContext) const
{
	if (!AssetCollectionType)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Collection type is not set."));
		return false;
	}

	return true;
}

UPCGExAssetCollection* FPCGExRoamingAssetCollectionDetails::TryBuildCollection(FPCGExContext* InContext, const UPCGParamData* InAttributeSet, const bool bBuildStaging) const
{
	if (!AssetCollectionType) { return nullptr; }
	UPCGExAssetCollection* Collection = InContext->ManagedObjects->New<UPCGExAssetCollection>(GetTransientPackage(), AssetCollectionType, NAME_None);
	if (!Collection) { return nullptr; }

	if (!Collection->BuildFromAttributeSet(InContext, InAttributeSet, *this, bBuildStaging))
	{
		InContext->ManagedObjects->Destroy(Collection);
		return nullptr;
	}

	return Collection;
}

UPCGExAssetCollection* FPCGExRoamingAssetCollectionDetails::TryBuildCollection(FPCGExContext* InContext, const FName InputPin, const bool bBuildStaging) const
{
	if (!AssetCollectionType) { return nullptr; }
	UPCGExAssetCollection* Collection = InContext->ManagedObjects->New<UPCGExAssetCollection>(GetTransientPackage(), AssetCollectionType, NAME_None);
	if (!Collection) { return nullptr; }

	if (!Collection->BuildFromAttributeSet(InContext, InputPin, *this, bBuildStaging))
	{
		InContext->ManagedObjects->Destroy(Collection);
		return nullptr;
	}

	return Collection;
}

namespace PCGExAssetCollection
{
}
