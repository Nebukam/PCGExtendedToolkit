// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExData.h"
#include "Collections/PCGExAssetCollection.h"

namespace PCGExStaging
{
	const FName SourceCollectionMapLabel = TEXT("Map");
	const FName OutputCollectionMapLabel = TEXT("Map");
	
	const FName Tag_CollectionPath = FName(PCGEx::PCGExPrefix + TEXT("Collection/Path"));
	const FName Tag_CollectionIdx = FName(PCGEx::PCGExPrefix + TEXT("Collection/Idx"));
	const FName Tag_EntryIdx = FName(PCGEx::PCGExPrefix + TEXT("CollectionEntry"));
	
	class FCollectionPickDatasetPacker : public TSharedFromThis<FCollectionPickDatasetPacker>
	{
		TArray<const UPCGExAssetCollection*> AssetCollections;
		TMap<const UPCGExAssetCollection*, uint16> CollectionMap;
		mutable FRWLock AssetCollectionsLock;

	public:
		FCollectionPickDatasetPacker()
		{
		}

		uint64 GetPickIdx(const UPCGExAssetCollection* InCollection, const int32 InIndex)
		{
			{
				FReadScopeLock ReadScopeLock(AssetCollectionsLock);
				if (const uint16* ColIndexPtr = CollectionMap.Find(InCollection)) { return PCGEx::H64(*ColIndexPtr, InIndex); }
			}

			{
				FWriteScopeLock WriteScopeLock(AssetCollectionsLock);
				if (const uint16* ColIndexPtr = CollectionMap.Find(InCollection)) { return PCGEx::H64(*ColIndexPtr, InIndex); }

				uint16 ColIndex = static_cast<uint16>(AssetCollections.Add(InCollection));
				CollectionMap.Add(InCollection, ColIndex);
				return PCGEx::H64(ColIndex, InIndex);
			}
		}

		void PackToDataset(const UPCGParamData* InAttributeSet)
		{
			FPCGMetadataAttribute<int32>* CollectionIdx = InAttributeSet->Metadata->FindOrCreateAttribute<int32>(Tag_CollectionIdx, 0, false, true, true);

#if PCGEX_ENGINE_VERSION > 503
			FPCGMetadataAttribute<FSoftObjectPath>* CollectionPath = InAttributeSet->Metadata->FindOrCreateAttribute<FSoftObjectPath>(Tag_CollectionPath, FSoftObjectPath(), false, true, true);
#else
			FPCGMetadataAttribute<FString>* CollectionPath  = InAttributeSet->Metadata->FindOrCreateAttribute<FString>(Tag_CollectionPath, TEXT(""), false, true, true);
#endif

			for (const TPair<const UPCGExAssetCollection*, uint16>& Pair : CollectionMap)
			{
				const int64 Key = InAttributeSet->Metadata->AddEntry();
				CollectionIdx->SetValue(Key, Pair.Value);

#if PCGEX_ENGINE_VERSION > 503
				CollectionPath->SetValue(Key, FSoftObjectPath(Pair.Key));
#else
				CollectionPath->SetValue(Key, FSoftObjectPath(Pair.Key).ToString());
#endif
			}
		}
	};

	template <typename C = UPCGExAssetCollection, typename A = FPCGExAssetCollectionEntry>
	class TCollectionPickDatasetUnpacker : public TSharedFromThis<TCollectionPickDatasetUnpacker<C, A>>
	{
		TMap<uint16, const C*> CollectionMap;

	public:
		TCollectionPickDatasetUnpacker()
		{
		}

		bool UnpackDataset(FPCGExContext* InContext, const UPCGParamData* InAttributeSet)
		{
			const UPCGMetadata* Metadata = InAttributeSet->Metadata;

#if PCGEX_ENGINE_VERSION > 503
			TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);
#else
			TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Infos->Attributes[0]); // Probably not reliable, but make 5.3 compile -_-
#endif

			const int32 NumEntries = Keys->GetNum();
			if (NumEntries == 0)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute set is empty."));
				return false;
			}

			CollectionMap.Reserve(NumEntries);

			const FPCGMetadataAttribute<int32>* CollectionIdx = InAttributeSet->Metadata->GetConstTypedAttribute<int32>(Tag_CollectionIdx);

#if PCGEX_ENGINE_VERSION > 503
			const FPCGMetadataAttribute<FSoftObjectPath>* CollectionPath = InAttributeSet->Metadata->GetConstTypedAttribute<FSoftObjectPath>(Tag_CollectionPath);
#else
			const FPCGMetadataAttribute<FString>* CollectionPath  = InParamData->Metadata->GetConstTypedAttribute<FString>(Tag_CollectionPath);
#endif

			if (!CollectionIdx || !CollectionPath)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing required attributes."));
				return false;
			}

			for (int i = 0; i < NumEntries; i++)
			{
				int32 Idx = CollectionIdx->GetValueFromItemKey(i);

#if PCGEX_ENGINE_VERSION > 503
				const C* Collection = TSoftObjectPtr<C>(CollectionPath->GetValueFromItemKey(i)).LoadSynchronous();
#else
				const C* Collection = TSoftObjectPtr<C>(FSoftObjectPath(CollectionPath->GetValueFromItemKey(i))).LoadSynchronous();
#endif

				if (!Collection)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Some collections could not be loaded."));
					return false;
				}

				CollectionMap.Add(Idx, Collection);
			}

			return true;
		}

		bool ResolveEntry(uint64 EntryHash, const A*& OutEntry)
		{
			const UPCGExAssetCollection* EntryHost = nullptr;
			const FPCGExAssetCollectionEntry* Entry = nullptr;

			uint32 CollectionIdx = 0;
			uint32 EntryIndex = 0;
			PCGEx::H64(EntryHash, CollectionIdx, EntryIndex);

			const C** Collection = CollectionMap.Find(EntryHash);
			if (!Collection || !Collection->IsValidIndex(EntryIndex)) { return false; }

			(*Collection)->GetEntryAt(OutEntry, EntryIndex, EntryHost);
			return true;
		}
	};
}
