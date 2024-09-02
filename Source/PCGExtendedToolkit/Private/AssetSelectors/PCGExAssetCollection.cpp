// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetSelectors/PCGExAssetCollection.h"

#include "PCGEx.h"
#include "PCGExMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"

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
	Staging.Weight = Weight;
	Staging.Category = Category;
}

void FPCGExAssetCollectionEntry::SetAssetPath(FSoftObjectPath InPath)
{
}

void FPCGExAssetCollectionEntry::OnSubCollectionLoaded()
{
}

namespace PCGExAssetCollection
{
	void FCache::FinalizeCache()
	{
		Shrink();

		const int32 NumEntries = Indices.Num();

		PCGEx::ArrayOfIndices(Order, NumEntries);

		Order.Sort([&](const int32 A, const int32 B) { return Weights[A] < Weights[B]; });
		Weights.Sort([&](const int32 A, const int32 B) { return A < B; });

		for (int32 i = 0; i < NumEntries; i++) { Weights[i] = i == 0 ? Weights[i] : Weights[i - 1] + Weights[i]; }
	}
}

PCGExAssetCollection::FCache* UPCGExAssetCollection::LoadCache()
{
	if (bCacheNeedsRebuild) { PCGEX_DELETE(Cache) }
	if (Cache) { return Cache; }
	Cache = new PCGExAssetCollection::FCache();
	BuildCache();
	Cache->FinalizeCache();
	return Cache;
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
}

#if WITH_EDITOR
void UPCGExAssetCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (EDITOR_IsCacheableProperty(PropertyChangedEvent)) { EDITOR_RebuildStagingData(); }

	EDITOR_RefreshDisplayNames();
	EDITOR_SetDirty();
}

void UPCGExAssetCollection::EDITOR_RefreshDisplayNames()
{
}

bool UPCGExAssetCollection::EDITOR_IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	return PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FPCGExAssetCollectionEntry, bIsSubCollection) ||
		PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FPCGExAssetCollectionEntry, Weight) ||
		PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FPCGExAssetCollectionEntry, Category);
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData()
{
	RebuildStagingData(false);
	Modify();
	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData_Recursive()
{
	RebuildStagingData(true);
	Modify();
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
#endif


void UPCGExAssetCollection::BeginDestroy()
{
	PCGEX_DELETE(Cache)
	Super::BeginDestroy();
}

void UPCGExAssetCollection::BuildCache()
{
	/* per-class implementation, forwards Entries to protected method */
}

UPCGExAssetCollection* UPCGExAssetCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return nullptr;
}

UPCGExAssetCollection* UPCGExAssetCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return nullptr;
}

void UPCGExAssetCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const
{
}

namespace PCGExAssetCollection
{
	FDistributionHelper::FDistributionHelper(
		UPCGExAssetCollection* InCollection,
		const FPCGExAssetDistributionDetails& InDetails):
		Collection(InCollection),
		Details(InDetails)
	{
	}

	bool FDistributionHelper::Init(
		const FPCGContext* InContext,
		PCGExData::FFacade* InDataFacade)
	{
		MaxIndex = InDataFacade->Source->GetNum() - 1;

		if (Details.Distribution == EPCGExDistribution::Index)
		{
			if (Details.IndexSettings.bRemapIndexToCollectionSize)
			{
				// Non-dynamic since we want min-max to start with :(
				IndexGetter = InDataFacade->GetBroadcaster<int32>(Details.IndexSettings.IndexSource, true);
			}
			else
			{
				IndexGetter = InDataFacade->GetScopedBroadcaster<int32>(Details.IndexSettings.IndexSource);
			}

			if (!IndexGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Invalid Index attribute used"));
				return false;
			}

			if (Details.IndexSettings.bRemapIndexToCollectionSize)
			{
				MaxInputIndex = static_cast<double>(IndexGetter->Max);
			}
		}

		return true;
	}

	void FDistributionHelper::GetStaging(const FPCGExAssetStagingData*& OutStaging, const int32 PointIndex, const int32 Seed) const
	{
		if (Details.Distribution == EPCGExDistribution::WeightedRandom)
		{
			Collection->GetStagingWeightedRandom(OutStaging, Seed);
		}
		else if (Details.Distribution == EPCGExDistribution::Random)
		{
			Collection->GetStagingRandom(OutStaging, Seed);
		}
		else
		{
			double PickedIndex = IndexGetter->Values[PointIndex];
			if (Details.IndexSettings.bRemapIndexToCollectionSize)
			{
				PickedIndex = MaxInputIndex == 0 ? 0 : PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex);
				switch (Details.IndexSettings.TruncateRemap)
				{
				case EPCGExTruncateMode::Round:
					PickedIndex = FMath::RoundToInt(PickedIndex);
					break;
				case EPCGExTruncateMode::Ceil:
					PickedIndex = FMath::CeilToDouble(PickedIndex);
					break;
				case EPCGExTruncateMode::Floor:
					PickedIndex = FMath::FloorToDouble(PickedIndex);
					break;
				default:
				case EPCGExTruncateMode::None:
					break;
				}
			}

			Collection->GetStaging(
				OutStaging,
				PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
				Seed, Details.IndexSettings.PickMode);
		}
	}
}
