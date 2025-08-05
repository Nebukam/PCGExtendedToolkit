// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Collections/PCGExAssetCollection.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

namespace PCGExStaging
{
	const FName SourceCollectionMapLabel = TEXT("Map");
	const FName OutputCollectionMapLabel = TEXT("Map");

	const FName Tag_CollectionPath = FName(PCGExCommon::PCGExPrefix + TEXT("Collection/Path"));
	const FName Tag_CollectionIdx = FName(PCGExCommon::PCGExPrefix + TEXT("Collection/Idx"));
	const FName Tag_EntryIdx = FName(PCGExCommon::PCGExPrefix + TEXT("CollectionEntry"));

	class PCGEXTENDEDTOOLKIT_API FPickPacker : public TSharedFromThis<FPickPacker>
	{
		FPCGExContext* Context = nullptr;

		TArray<const UPCGExAssetCollection*> AssetCollections;
		TMap<const UPCGExAssetCollection*, uint32> CollectionMap;
		mutable FRWLock AssetCollectionsLock;

		uint16 BaseHash = 0;

	public:
		FPickPacker(FPCGExContext* InContext);


		uint64 GetPickIdx(const UPCGExAssetCollection* InCollection, const int16 InIndex, const int16 InSecondaryIndex);

		void PackToDataset(const UPCGParamData* InAttributeSet);
	};

	class PCGEXTENDEDTOOLKIT_API IPickUnpacker : public TSharedFromThis<IPickUnpacker>
	{
	protected:
		TMap<uint32, UPCGExAssetCollection*> CollectionMap;
		int32 NumUniqueEntries = 0;
		const UPCGBasePointData* PointData = nullptr;

	public:
		TMap<int64, TSharedPtr<TArray<int32>>> HashedPartitions;
		TMap<int64, int32> IndexedPartitions;

		IPickUnpacker() = default;

		bool HasValidMapping() const { return !CollectionMap.IsEmpty(); }

		bool UnpackDataset(FPCGContext* InContext, const UPCGParamData* InAttributeSet);
		void UnpackPin(FPCGContext* InContext, const FName InPinLabel);

		bool BuildPartitions(const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& InstanceLists);
		void RetrievePartitions(const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& InstanceLists);
		void InsertEntry(const uint64 EntryHash, const int32 EntryIndex, TArray<FPCGMeshInstanceList>& InstanceLists);

		UPCGExAssetCollection* UnpackHash(const uint64 EntryHash, int16& OutPrimaryIndex, int16& OutSecondaryIndex);
	};

	template <typename C = UPCGExAssetCollection, typename A = FPCGExAssetCollectionEntry>
	class PCGEXTENDEDTOOLKIT_API TPickUnpacker : public IPickUnpacker
	{
	public:
		TPickUnpacker() = default;

		bool ResolveEntry(const uint64 EntryHash, const A*& OutEntry, int16& OutSecondaryIndex)
		{
			const UPCGExAssetCollection* EntryHost = nullptr;

			int16 EntryIndex = 0;
			UPCGExAssetCollection* Collection = UnpackHash(EntryHash, EntryIndex, OutSecondaryIndex);
			if (!Collection) { return false; }

			return static_cast<C*>(Collection)->GetEntryAt(OutEntry, EntryIndex, EntryHost);
		}
	};

	class PCGEXTENDEDTOOLKIT_API IDistributionHelper : public TSharedFromThis<IDistributionHelper>
	{
	protected:
		PCGExAssetCollection::FCache* Cache = nullptr;
		UPCGExAssetCollection* MainCollection = nullptr;

		TSharedPtr<PCGExDetails::TSettingValue<int32>> IndexGetter;
		TSharedPtr<PCGExDetails::TSettingValue<FName>> CategoryGetter;

		double MaxInputIndex = 0;

	public:
		FPCGExAssetDistributionDetails Details;

		IDistributionHelper(UPCGExAssetCollection* InCollection, const FPCGExAssetDistributionDetails& InDetails);
		bool Init(const TSharedRef<PCGExData::FFacade>& InDataFacade);
	};

	template <typename C = UPCGExAssetCollection, typename A = FPCGExAssetCollectionEntry>
	class PCGEXTENDEDTOOLKIT_API TDistributionHelper : public IDistributionHelper
	{
	public:
		C* TypedCollection = nullptr;

		TDistributionHelper(C* InCollection, const FPCGExAssetDistributionDetails& InDetails)
			: IDistributionHelper(InCollection, InDetails)
		{
			TypedCollection = InCollection;
		}

		void GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, const UPCGExAssetCollection*& OutHost) const
		{
			TSharedPtr<PCGExAssetCollection::FCategory> Category = Cache->Main;
			C* WorkingCollection = TypedCollection;

			if (CategoryGetter)
			{
				TSharedPtr<PCGExAssetCollection::FCategory>* CategoryPtr = Cache->Categories.Find(CategoryGetter->Read(PointIndex));

				if (!CategoryPtr)
				{
					OutEntry = nullptr;
					return;
				}

				Category = *CategoryPtr;

				if (Category->IsEmpty())
				{
					OutEntry = nullptr;
					return;
				}

				if (Category->Num() == 1)
				{
					// Single-item category
					TypedCollection->GetEntryAt(OutEntry, Category->Indices[0], OutHost);
				}
				else
				{
					// Multi-item category
					TypedCollection->GetEntryAt(OutEntry, Category->GetPickRandomWeighted(Seed), OutHost);
				}

				WorkingCollection = (OutEntry && OutEntry->bIsSubCollection) ? static_cast<C*>(OutEntry->InternalSubCollection.Get()) : nullptr;
				if (!WorkingCollection) { return; }
			}


			if (Details.Distribution == EPCGExDistribution::WeightedRandom)
			{
				WorkingCollection->GetEntryWeightedRandom(OutEntry, Seed, OutHost);
			}
			else if (Details.Distribution == EPCGExDistribution::Random)
			{
				WorkingCollection->GetEntryRandom(OutEntry, Seed, OutHost);
			}
			else
			{
				const int32 MaxIndex = WorkingCollection->LoadCache()->Main->Num() - 1;
				double PickedIndex = IndexGetter->Read(PointIndex);
				if (Details.IndexSettings.bRemapIndexToCollectionSize)
				{
					PickedIndex = PCGEx::TruncateDbl(
						MaxInputIndex == 0 ? 0 : PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex),
						Details.IndexSettings.TruncateRemap);
				}

				WorkingCollection->GetEntry(
					OutEntry,
					PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
					Seed, Details.IndexSettings.PickMode, OutHost);
			}
		}

		void GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, const uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) const
		{
			if (TagInheritance == 0)
			{
				GetEntry(OutEntry, PointIndex, Seed, OutHost);
				return;
			}

			TSharedPtr<PCGExAssetCollection::FCategory> Category = Cache->Main;
			C* WorkingCollection = TypedCollection;

			if (CategoryGetter)
			{
				TSharedPtr<PCGExAssetCollection::FCategory>* CategoryPtr = Cache->Categories.Find(CategoryGetter->Read(PointIndex));

				if (!CategoryPtr)
				{
					OutEntry = nullptr;
					return;
				}

				Category = *CategoryPtr;

				if (Category->IsEmpty())
				{
					OutEntry = nullptr;
					return;
				}

				if (Category->Num() == 1)
				{
					// Single-item category
					TypedCollection->GetEntryAt(OutEntry, Category->Indices[0], TagInheritance, OutTags, OutHost);
				}
				else
				{
					// Multi-item category
					TypedCollection->GetEntryAt(OutEntry, Category->GetPickRandomWeighted(Seed), TagInheritance, OutTags, OutHost);
				}

				WorkingCollection = (OutEntry && OutEntry->bIsSubCollection) ? static_cast<C*>(OutEntry->InternalSubCollection.Get()) : nullptr;
				if (!WorkingCollection) { return; }
			}

			if (Details.Distribution == EPCGExDistribution::WeightedRandom)
			{
				WorkingCollection->GetEntryWeightedRandom(OutEntry, Seed, TagInheritance, OutTags, OutHost);
			}
			else if (Details.Distribution == EPCGExDistribution::Random)
			{
				WorkingCollection->GetEntryRandom(OutEntry, Seed, TagInheritance, OutTags, OutHost);
			}
			else
			{
				const int32 MaxIndex = WorkingCollection->LoadCache()->Main->Num() - 1;
				double PickedIndex = IndexGetter->Read(PointIndex);
				if (Details.IndexSettings.bRemapIndexToCollectionSize)
				{
					PickedIndex = PCGEx::TruncateDbl(
						MaxInputIndex == 0 ? 0 : PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex),
						Details.IndexSettings.TruncateRemap);
				}

				WorkingCollection->GetEntry(
					OutEntry,
					PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
					Seed, Details.IndexSettings.PickMode, TagInheritance, OutTags, OutHost);
			}
		}
	};
}
