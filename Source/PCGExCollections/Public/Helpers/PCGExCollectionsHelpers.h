// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Core/PCGExAssetCollection.h"
#include "Helpers/PCGExSocketHelpers.h"

namespace PCGExData
{
	class FPointIOCollection;
	class FFacade;
}

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

struct FPCGContext;
struct FPCGMeshInstanceList;
class UPCGBasePointData;
class UPCGParamData;

namespace PCGExCollections
{
	/**
	 * Non-templated distribution helper.
	 * Works with any collection type through the polymorphic API.
	 * Use FPCGExEntryAccessResult::As<T>() when you need type-specific entry data.
	 */
	class PCGEXCOLLECTIONS_API FDistributionHelper : public TSharedFromThis<FDistributionHelper>
	{
	protected:
		PCGExAssetCollection::FCache* Cache = nullptr;
		UPCGExAssetCollection* Collection = nullptr;

		TSharedPtr<PCGExDetails::TSettingValue<int32>> IndexGetter;
		TSharedPtr<PCGExDetails::TSettingValue<FName>> CategoryGetter;
		double MaxInputIndex = 0;

	public:
		FPCGExAssetDistributionDetails Details;

		explicit FDistributionHelper(UPCGExAssetCollection* InCollection, const FPCGExAssetDistributionDetails& InDetails);

		/**
		 * Initialize the helper with a data facade
		 * @return true if initialization successful
		 */
		bool Init(const TSharedRef<PCGExData::FFacade>& InDataFacade);

		/**
		 * Get an entry for a specific point
		 * @param PointIndex Index of the point
		 * @param Seed Random seed for this point
		 * @return Access result containing entry and host collection
		 */
		FPCGExEntryAccessResult GetEntry(int32 PointIndex, int32 Seed) const;

		/**
		 * Get an entry with tag inheritance
		 * @param PointIndex Index of the point
		 * @param Seed Random seed for this point
		 * @param TagInheritance Bitmask of EPCGExAssetTagInheritance flags
		 * @param OutTags Set to append inherited tags to
		 * @return Access result containing entry and host collection
		 */
		FPCGExEntryAccessResult GetEntry(int32 PointIndex, int32 Seed, uint8 TagInheritance, TSet<FName>& OutTags) const;

		/** Get the underlying collection */
		UPCGExAssetCollection* GetCollection() const { return Collection; }

		/** Get the collection's type ID */
		PCGExAssetCollection::FTypeId GetCollectionTypeId() const
		{
			return Collection ? Collection->GetTypeId() : PCGExAssetCollection::TypeIds::None;
		}
	};

	/**
	 * Helper for distributing within an entry's MicroCache
	 * (e.g., selecting material variants)
	 */
	class PCGEXCOLLECTIONS_API FMicroDistributionHelper : public TSharedFromThis<FMicroDistributionHelper>
	{
	protected:
		TSharedPtr<PCGExDetails::TSettingValue<int32>> IndexGetter;
		double MaxInputIndex = 0;

	public:
		FPCGExMicroCacheDistributionDetails Details;

		explicit FMicroDistributionHelper(const FPCGExMicroCacheDistributionDetails& InDetails);

		bool Init(const TSharedRef<PCGExData::FFacade>& InDataFacade);

		/**
		 * Get a pick index from a MicroCache
		 * @param InMicroCache The MicroCache to pick from
		 * @param PointIndex Index of the point
		 * @param Seed Random seed
		 * @return The picked index, or -1 if invalid
		 */
		int32 GetPick(const PCGExAssetCollection::FMicroCache* InMicroCache, int32 PointIndex, int32 Seed) const;
	};

	/**
	 * Packs collection references and entry picks into an attribute set
	 * for downstream consumption (e.g., by mesh spawner)
	 */
	class PCGEXCOLLECTIONS_API FPickPacker : public TSharedFromThis<FPickPacker>
	{
		TArray<const UPCGExAssetCollection*> AssetCollections;
		TMap<const UPCGExAssetCollection*, uint32> CollectionMap;
		mutable FRWLock AssetCollectionsLock;

		uint16 BaseHash = 0;

	public:
		explicit FPickPacker(FPCGContext* InContext);

		/**
		 * Get a packed index for a collection entry pick
		 * @param InCollection The collection
		 * @param InIndex Primary entry index
		 * @param InSecondaryIndex Secondary index (e.g., material variant)
		 * @return Packed 64-bit identifier
		 */
		uint64 GetPickIdx(const UPCGExAssetCollection* InCollection, int16 InIndex, int16 InSecondaryIndex);

		/** Write collection mapping to an attribute set */
		void PackToDataset(const UPCGParamData* InAttributeSet);
	};

	/**
	 * Unpacks collection references and entry picks from an attribute set
	 */
	class PCGEXCOLLECTIONS_API FPickUnpacker : public TSharedFromThis<FPickUnpacker>
	{
	protected:
		TMap<uint32, UPCGExAssetCollection*> CollectionMap;
		TSharedPtr<struct FStreamableHandle> CollectionsHandle;
		int32 NumUniqueEntries = 0;
		const UPCGBasePointData* PointData = nullptr;

	public:
		TMap<int64, TSharedPtr<TArray<int32>>> HashedPartitions;
		TMap<int64, int32> IndexedPartitions;

		FPickUnpacker() = default;
		~FPickUnpacker();

		bool HasValidMapping() const { return !CollectionMap.IsEmpty(); }

		/** Unpack collection mappings from an attribute set */
		bool UnpackDataset(FPCGContext* InContext, const UPCGParamData* InAttributeSet);

		/** Unpack from a specific input pin */
		void UnpackPin(FPCGContext* InContext, FName InPinLabel);

		/** Build point partitions from point data */
		bool BuildPartitions(const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& InstanceLists);

		void InsertEntry(const uint64 EntryHash, const int32 EntryIndex, TArray<FPCGMeshInstanceList>& InstanceLists);

		/**
		 * Resolve a packed hash to an entry
		 * @param EntryHash The packed hash
		 * @param OutPrimaryIndex Output: primary entry index
		 * @param OutSecondaryIndex Output: secondary index
		 * @return The collection, or nullptr if not found
		 */
		UPCGExAssetCollection* UnpackHash(uint64 EntryHash, int16& OutPrimaryIndex, int16& OutSecondaryIndex);

		/**
		 * Resolve an entry from a packed hash
		 * @param EntryHash The packed hash
		 * @param OutSecondaryIndex Output: secondary index
		 * @return Entry access result
		 */
		FPCGExEntryAccessResult ResolveEntry(uint64 EntryHash, int16& OutSecondaryIndex);
	};

	/**
	 * Manages collection source(s) for staging operations.
	 * Supports both single collection and per-point collection mapping.
	 */
	class PCGEXCOLLECTIONS_API FCollectionSource : public TSharedFromThis<FCollectionSource>
	{
		TSharedPtr<FDistributionHelper> Helper;
		TSharedPtr<FMicroDistributionHelper> MicroHelper;

		// For mapped sources
		TArray<TSharedPtr<FDistributionHelper>> Helpers;
		TArray<TSharedPtr<FMicroDistributionHelper>> MicroHelpers;
		TMap<PCGExValueHash, int32> Indices;

		TSharedPtr<TArray<PCGExValueHash>> Keys;
		TSharedPtr<PCGExData::FFacade> DataFacade;
		UPCGExAssetCollection* SingleSource = nullptr;

	public:
		FPCGExAssetDistributionDetails DistributionSettings;
		FPCGExMicroCacheDistributionDetails EntryDistributionSettings;

		explicit FCollectionSource(const TSharedPtr<PCGExData::FFacade>& InDataFacade);

		/** Initialize with a single collection */
		bool Init(UPCGExAssetCollection* InCollection);

		/** Initialize with a mapped collection source */
		bool Init(const TMap<PCGExValueHash, TObjectPtr<UPCGExAssetCollection>>& InMap, const TSharedPtr<TArray<PCGExValueHash>>& InKeys);

		/**
		 * Get helpers for a specific point index
		 * @param Index Point index
		 * @param OutHelper Output: distribution helper
		 * @param OutMicroHelper Output: micro distribution helper (may be null)
		 * @return true if valid helpers found
		 */
		bool TryGetHelpers(int32 Index, FDistributionHelper*& OutHelper, FMicroDistributionHelper*& OutMicroHelper);

		/** Check if this is a single source */
		bool IsSingleSource() const { return SingleSource != nullptr; }

		/** Get the single source collection (if applicable) */
		UPCGExAssetCollection* GetSingleSource() const { return SingleSource; }
	};

	class PCGEXCOLLECTIONS_API FSocketHelper : public PCGExStaging::FSocketHelper
	{
	public:
		explicit FSocketHelper(const FPCGExSocketOutputDetails* InDetails, const int32 InNumPoints);

		void Add(const int32 Index, const uint64 EntryHash, const FPCGExAssetCollectionEntry* Entry);
	};
}
