// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExAssetCollection.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

struct FPCGExMeshCollectionEntry;
class UPCGExMeshCollection;
class UPCGExActorCollection;

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

		bool ResolveEntry(const uint64 EntryHash, const A*& OutEntry, int16& OutSecondaryIndex);
	};

	extern template class TPickUnpacker<UPCGExAssetCollection, FPCGExAssetCollectionEntry>;
	extern template class TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>;
	extern template class TPickUnpacker<UPCGExActorCollection, FPCGExActorCollectionEntry>;

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

		TDistributionHelper(C* InCollection, const FPCGExAssetDistributionDetails& InDetails);

		void GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, const UPCGExAssetCollection*& OutHost) const;
		void GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, const uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) const;
	};

	extern template class TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>;
	extern template class TDistributionHelper<UPCGExMeshCollection, FPCGExMeshCollectionEntry>;
	extern template class TDistributionHelper<UPCGExActorCollection, FPCGExActorCollectionEntry>;

	struct PCGEXTENDEDTOOLKIT_API FSocketInfos
	{
		FSocketInfos() = default;
		const FPCGExAssetCollectionEntry* Entry = nullptr;
		int32 Count = 0;
		int32 SocketCount = 0;
	};

	PCGEXTENDEDTOOLKIT_API
	uint64 GetSimplifiedEntryHash(uint64 InEntryHash);

	class PCGEXTENDEDTOOLKIT_API FSocketHelper
	{
		FRWLock EntryMapLock;

		const FPCGExSocketOutputDetails* Details = nullptr;

		TMap<uint64, FSocketInfos> EntryMap;
		TMap<uint64, int32> SocketCount;
		int32 NumOutPoints = 0;

	public:
		explicit FSocketHelper(const FPCGExSocketOutputDetails* InDetails);
		void Add(const TMap<uint64, FSocketInfos>& InEntryMap);
		int32 Compile();
	};
}
