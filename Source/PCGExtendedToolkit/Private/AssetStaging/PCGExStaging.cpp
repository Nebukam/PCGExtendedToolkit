// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExStaging.h"

#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

namespace PCGExStaging
{
	FPickPacker::FPickPacker(FPCGExContext* InContext)
		: Context(InContext)
	{
		BaseHash = static_cast<uint16>(InContext->GetInputSettings<UPCGSettings>()->UID);
	}

	uint64 FPickPacker::GetPickIdx(const UPCGExAssetCollection* InCollection, const int16 InIndex, const int16 InSecondaryIndex)
	{
		const uint32 ItemHash = PCGEx::H32(InIndex, InSecondaryIndex + 1);

		{
			FReadScopeLock ReadScopeLock(AssetCollectionsLock);
			if (const uint32* ColIdxPtr = CollectionMap.Find(InCollection)) { return PCGEx::H64(*ColIdxPtr, ItemHash); }
		}

		{
			FWriteScopeLock WriteScopeLock(AssetCollectionsLock);
			if (const uint32* ColIdxPtr = CollectionMap.Find(InCollection)) { return PCGEx::H64(*ColIdxPtr, ItemHash); }

			uint32 ColIndex = PCGEx::H32(BaseHash, AssetCollections.Add(InCollection));
			CollectionMap.Add(InCollection, ColIndex);
			return PCGEx::H64(ColIndex, ItemHash);
		}
	}

	void FPickPacker::PackToDataset(const UPCGParamData* InAttributeSet)
	{
		FPCGMetadataAttribute<int32>* CollectionIdx = InAttributeSet->Metadata->FindOrCreateAttribute<int32>(Tag_CollectionIdx, 0, false, true, true);
		FPCGMetadataAttribute<FSoftObjectPath>* CollectionPath = InAttributeSet->Metadata->FindOrCreateAttribute<FSoftObjectPath>(Tag_CollectionPath, FSoftObjectPath(), false, true, true);

		for (const TPair<const UPCGExAssetCollection*, uint32>& Pair : CollectionMap)
		{
			const int64 Key = InAttributeSet->Metadata->AddEntry();
			CollectionIdx->SetValue(Key, Pair.Value);
			CollectionPath->SetValue(Key, FSoftObjectPath(Pair.Key));
		}
	}

	bool IPickUnpacker::UnpackDataset(FPCGContext* InContext, const UPCGParamData* InAttributeSet)
	{
		const UPCGMetadata* Metadata = InAttributeSet->Metadata;
		TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);

		const int32 NumEntries = Keys->GetNum();
		if (NumEntries == 0)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attribute set is empty."));
			return false;
		}

		CollectionMap.Reserve(CollectionMap.Num() + NumEntries);

		const FPCGMetadataAttribute<int32>* CollectionIdx = InAttributeSet->Metadata->GetConstTypedAttribute<int32>(Tag_CollectionIdx);
		const FPCGMetadataAttribute<FSoftObjectPath>* CollectionPath = InAttributeSet->Metadata->GetConstTypedAttribute<FSoftObjectPath>(Tag_CollectionPath);

		if (!CollectionIdx || !CollectionPath)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing required attributes, or unsupported type."));
			return false;
		}

		for (int i = 0; i < NumEntries; i++)
		{
			int32 Idx = CollectionIdx->GetValueFromItemKey(i);

			UPCGExAssetCollection* Collection = PCGExHelpers::LoadBlocking_AnyThread<UPCGExAssetCollection>(TSoftObjectPtr<UPCGExAssetCollection>(CollectionPath->GetValueFromItemKey(i)));

			if (!Collection)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Some collections could not be loaded."));
				return false;
			}

			if (CollectionMap.Contains(Idx))
			{
				if (CollectionMap[Idx] == Collection) { continue; }

				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Collection Idx collision."));
				return false;
			}

			CollectionMap.Add(Idx, Collection);
			NumUniqueEntries += Collection->GetValidEntryNum();
		}

		return true;
	}

	void IPickUnpacker::UnpackPin(FPCGContext* InContext, const FName InPinLabel)
	{
		for (TArray<FPCGTaggedData> Params = InContext->InputData.GetParamsByPin(InPinLabel);
		     const FPCGTaggedData& InTaggedData : Params)
		{
			const UPCGParamData* ParamData = Cast<UPCGParamData>(InTaggedData.Data);

			if (!ParamData) { continue; }
			const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(ParamData->Metadata);

			if (!ParamData->Metadata->HasAttribute(Tag_CollectionIdx) || !ParamData->Metadata->HasAttribute(Tag_CollectionPath)) { continue; }

			UnpackDataset(InContext, ParamData);
		}
	}

	bool IPickUnpacker::BuildPartitions(const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& InstanceLists)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TPickUnpacker::BuildPartitions_Indexed);

		FPCGAttributePropertyInputSelector HashSelector;
		HashSelector.Update(Tag_EntryIdx.ToString());

		TUniquePtr<const IPCGAttributeAccessor> HashAttributeAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InPointData, HashSelector);
		TUniquePtr<const IPCGAttributeAccessorKeys> HashKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InPointData, HashSelector);

		if (!HashAttributeAccessor || !HashKeys) { return false; }

		TArray<int64> Hashes;
		Hashes.SetNumUninitialized(HashKeys->GetNum());

		if (!HashAttributeAccessor->GetRange<int64>(Hashes, 0, *HashKeys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
		{
			return false;
		}

		const int32 NumPoints = InPointData->GetNumPoints();
		const int32 SafeReserve = NumPoints / (NumUniqueEntries * 2);

		// Build partitions
		for (int i = 0; i < NumPoints; i++)
		{
			const uint64 EntryHash = Hashes[i];
			if (const int32* Index = IndexedPartitions.Find(EntryHash); !Index)
			{
				FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef();
				NewInstanceList.AttributePartitionIndex = EntryHash;
				NewInstanceList.PointData = InPointData;
				NewInstanceList.InstancesIndices.Reserve(SafeReserve);
				NewInstanceList.InstancesIndices.Emplace(i);

				IndexedPartitions.Add(EntryHash, InstanceLists.Num() - 1);
			}
			else
			{
				InstanceLists[*Index].InstancesIndices.Emplace(i);
			}
		}

		return !IndexedPartitions.IsEmpty();
	}

	void IPickUnpacker::RetrievePartitions(const UPCGBasePointData* InPointData, TArray<FPCGMeshInstanceList>& InstanceLists)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TPickUnpacker::BuildPartitions_Indexed);

		PointData = InPointData;

		for (FPCGMeshInstanceList& InstanceList : InstanceLists)
		{
			IndexedPartitions.Add(InstanceList.AttributePartitionIndex, InstanceLists.Num() - 1);
		}
	}

	void IPickUnpacker::InsertEntry(const uint64 EntryHash, const int32 EntryIndex, TArray<FPCGMeshInstanceList>& InstanceLists)
	{
		if (const int32* Index = IndexedPartitions.Find(EntryHash); !Index)
		{
			FPCGMeshInstanceList& NewInstanceList = InstanceLists.Emplace_GetRef();
			NewInstanceList.AttributePartitionIndex = EntryHash;
			NewInstanceList.PointData = PointData;
			NewInstanceList.InstancesIndices.Reserve(PointData->GetNumPoints() / (NumUniqueEntries * 2));
			NewInstanceList.InstancesIndices.Emplace(EntryIndex);

			IndexedPartitions.Add(EntryHash, InstanceLists.Num() - 1);
		}
		else
		{
			InstanceLists[*Index].InstancesIndices.Emplace(EntryIndex);
		}
	}

	UPCGExAssetCollection* IPickUnpacker::UnpackHash(const uint64 EntryHash, int16& OutPrimaryIndex, int16& OutSecondaryIndex)
	{
		uint32 CollectionIdx = 0;
		uint32 OutEntryIndices = 0;

		PCGEx::H64(EntryHash, CollectionIdx, OutEntryIndices);

		uint16 EntryIndex = 0;
		uint16 SecondaryIndex = 0;

		PCGEx::H32(OutEntryIndices, EntryIndex, SecondaryIndex);
		OutSecondaryIndex = SecondaryIndex - 1; // minus one because we do +1 during packing

		UPCGExAssetCollection** Collection = CollectionMap.Find(CollectionIdx);
		if (!Collection || !(*Collection)->IsValidIndex(EntryIndex)) { return nullptr; }

		OutPrimaryIndex = EntryIndex;

		return *Collection;
	}

	IDistributionHelper::IDistributionHelper(UPCGExAssetCollection* InCollection, const FPCGExAssetDistributionDetails& InDetails)
		: MainCollection(InCollection), Details(InDetails)
	{
	}

	bool IDistributionHelper::Init(const TSharedRef<PCGExData::FFacade>& InDataFacade)
	{
		Cache = MainCollection->LoadCache();

		if (Cache->IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InDataFacade->GetContext(), FTEXT("TDistributionHelper got an empty Collection."));
			return false;
		}

		if (Details.bUseCategories)
		{
			CategoryGetter = Details.GetValueSettingCategory();
			if (!CategoryGetter->Init(InDataFacade)) { return false; }
		}

		if (Details.Distribution == EPCGExDistribution::Index)
		{
			const bool bWantsMinMax = Details.IndexSettings.bRemapIndexToCollectionSize;

			IndexGetter = Details.IndexSettings.GetValueSettingIndex();
			if (!IndexGetter->Init(InDataFacade, !bWantsMinMax, bWantsMinMax)) { return false; }

			MaxInputIndex = IndexGetter->Max();
		}

		return true;
	}

	uint64 GetSimplifiedEntryHash(const uint64 InEntryHash)
	{
		uint32 CollectionIdx = 0;
		uint32 OutEntryIndices = 0;
		PCGEx::H64(InEntryHash, CollectionIdx, OutEntryIndices);
		return PCGEx::H64(CollectionIdx, PCGEx::H32A(OutEntryIndices));
	}

	template <typename C, typename A>
	bool TPickUnpacker<C, A>::ResolveEntry(const uint64 EntryHash, const A*& OutEntry, int16& OutSecondaryIndex)
	{
		const UPCGExAssetCollection* EntryHost = nullptr;

		int16 EntryIndex = 0;
		UPCGExAssetCollection* Collection = UnpackHash(EntryHash, EntryIndex, OutSecondaryIndex);
		if (!Collection) { return false; }

		return static_cast<C*>(Collection)->GetEntryAt(OutEntry, EntryIndex, EntryHost);
	}

	template PCGEXTENDEDTOOLKIT_API class TPickUnpacker<UPCGExAssetCollection, FPCGExAssetCollectionEntry>;
	template PCGEXTENDEDTOOLKIT_API class TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>;
	template PCGEXTENDEDTOOLKIT_API class TPickUnpacker<UPCGExActorCollection, FPCGExActorCollectionEntry>;

	template <typename C, typename A>
	TDistributionHelper<C, A>::TDistributionHelper(C* InCollection, const FPCGExAssetDistributionDetails& InDetails)
		: IDistributionHelper(InCollection, InDetails)
	{
		TypedCollection = InCollection;
	}

	template <typename C, typename A>
	void TDistributionHelper<C, A>::GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, const UPCGExAssetCollection*& OutHost) const
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

	template <typename C, typename A>
	void TDistributionHelper<C, A>::GetEntry(const A*& OutEntry, const int32 PointIndex, const int32 Seed, const uint8 TagInheritance, TSet<FName>& OutTags, const UPCGExAssetCollection*& OutHost) const
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

	template PCGEXTENDEDTOOLKIT_API class TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>;
	template PCGEXTENDEDTOOLKIT_API class TDistributionHelper<UPCGExMeshCollection, FPCGExMeshCollectionEntry>;
	template PCGEXTENDEDTOOLKIT_API class TDistributionHelper<UPCGExActorCollection, FPCGExActorCollectionEntry>;


	FSocketHelper::FSocketHelper(const FPCGExSocketOutputDetails* InDetails)
		:Details(InDetails)
	{
	}

	void FSocketHelper::Add(const TMap<uint64, FSocketInfos>& InEntryMap)
	{
		FWriteScopeLock WriteScope(EntryMapLock);

		for (const TPair<uint64, FSocketInfos>& InInfos : InEntryMap)
		{
			FSocketInfos* ExistingInfos = EntryMap.Find(InInfos.Key);

			if (!ExistingInfos)
			{
				FSocketInfos& NewInfos = EntryMap.Add(InInfos.Key, *ExistingInfos);
				// TODO : This is a new entry, we can pre-compute number of sockets already
				NewInfos.SocketCount = InInfos.Value.Entry->Staging.Sockets.Num();;
				continue;
			}

			ExistingInfos->Count += InInfos.Value.Count;
		}
	}

	int32 FSocketHelper::Compile()
	{
		NumOutPoints = 0;

		for (const TPair<uint64, FSocketInfos>& InInfos : EntryMap)
		{
			// TODO : Compute number of "valid" sockets
			NumOutPoints += InInfos.Value.Count * InInfos.Value.SocketCount;
		}

		return NumOutPoints;
	}
}
