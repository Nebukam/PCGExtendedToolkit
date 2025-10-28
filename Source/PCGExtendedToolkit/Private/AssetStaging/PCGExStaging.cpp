// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssetStaging/PCGExStaging.h"

#include "PCGExGlobalSettings.h"
#include "PCGExRandom.h"
#include "Engine/StaticMeshSocket.h"
#include "Collections/PCGExAssetCollection.h"
#include "Collections/PCGExActorCollection.h"
#include "Collections/PCGExMeshCollection.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsSettings.h"
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
		return (InEntryHash & 0xFFFFFFFF00000000ull) | ((InEntryHash >> 16) & 0xFFFF);
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

	template class PCGEXTENDEDTOOLKIT_API TPickUnpacker<UPCGExAssetCollection, FPCGExAssetCollectionEntry>;
	template class PCGEXTENDEDTOOLKIT_API TPickUnpacker<UPCGExMeshCollection, FPCGExMeshCollectionEntry>;
	template class PCGEXTENDEDTOOLKIT_API TPickUnpacker<UPCGExActorCollection, FPCGExActorCollectionEntry>;

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
				PickedIndex = PCGExMath::TruncateDbl(
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
				PickedIndex = PCGExMath::TruncateDbl(
					MaxInputIndex == 0 ? 0 : PCGExMath::Remap(PickedIndex, 0, MaxInputIndex, 0, MaxIndex),
					Details.IndexSettings.TruncateRemap);
			}

			WorkingCollection->GetEntry(
				OutEntry,
				PCGExMath::SanitizeIndex(static_cast<int32>(PickedIndex), MaxIndex, Details.IndexSettings.IndexSafety),
				Seed, Details.IndexSettings.PickMode, TagInheritance, OutTags, OutHost);
		}
	}

	template class PCGEXTENDEDTOOLKIT_API TDistributionHelper<UPCGExAssetCollection, FPCGExAssetCollectionEntry>;
	template class PCGEXTENDEDTOOLKIT_API TDistributionHelper<UPCGExMeshCollection, FPCGExMeshCollectionEntry>;
	template class PCGEXTENDEDTOOLKIT_API TDistributionHelper<UPCGExActorCollection, FPCGExActorCollectionEntry>;


	FSocketHelper::FSocketHelper(const FPCGExSocketOutputDetails* InDetails, const int32 InNumPoints)
		: Details(InDetails)
	{
		Mapping.Init(-1, InNumPoints);
		StartIndices.Init(-1, InNumPoints);
	}

	void FSocketHelper::Add(const int32 Index, const uint64 EntryHash, const FPCGExAssetCollectionEntry* Entry)
	{
		const int32* IdxPtr = nullptr;

		{
			FReadScopeLock ReadLock(SocketLock);
			IdxPtr = InfosKeys.Find(EntryHash);
		}

		int32 Idx = -1;

		if (!IdxPtr)
		{
			FWriteScopeLock WriteLock(SocketLock);

			IdxPtr = InfosKeys.Find(EntryHash);
			if (IdxPtr)
			{
				Idx = *IdxPtr;
			}
			else
			{
				FSocketInfos& NewInfos = NewSocketInfos(EntryHash, Idx);

				NewInfos.Path = Entry->Staging.Path;
				NewInfos.Category = Entry->Category;
				NewInfos.Sockets = Entry->Staging.Sockets;

				FilterSocketInfos(Idx);
			}
		}
		else
		{
			Idx = *IdxPtr;
		}

		FPlatformAtomics::InterlockedIncrement(&SocketInfosList[Idx].Count);
		Mapping[Index] = Idx;
	}

	void FSocketHelper::Add(const int32 Index, const TObjectPtr<UStaticMesh>& Mesh)
	{
		const uint64 EntryHash = GetTypeHash(Mesh);

		const int32* IdxPtr = nullptr;

		{
			FReadScopeLock ReadLock(SocketLock);
			IdxPtr = InfosKeys.Find(EntryHash);
		}

		int32 Idx = -1;

		if (!IdxPtr)
		{
			FWriteScopeLock WriteLock(SocketLock);

			IdxPtr = InfosKeys.Find(EntryHash);

			if (IdxPtr)
			{
				Idx = *IdxPtr;
			}
			else
			{
				FSocketInfos& NewInfos = NewSocketInfos(EntryHash, Idx);

				NewInfos.Path = Mesh.GetPath();
				NewInfos.Category = NAME_None;
				NewInfos.Sockets.Reserve(Mesh->Sockets.Num());

				for (const TObjectPtr<UStaticMeshSocket>& MeshSocket : Mesh->Sockets)
				{
					FPCGExSocket& NewSocket = NewInfos.Sockets.Emplace_GetRef(MeshSocket->SocketName, MeshSocket->RelativeLocation, MeshSocket->RelativeRotation, MeshSocket->RelativeScale, MeshSocket->Tag);
					NewSocket.bManaged = true;
				}

				FilterSocketInfos(Idx);
			}
		}
		else
		{
			Idx = *IdxPtr;
		}

		FPlatformAtomics::InterlockedIncrement(&SocketInfosList[Idx].Count);
		Mapping[Index] = Idx;
	}

	void FSocketHelper::Compile(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		const TSharedPtr<PCGExData::FPointIOCollection>& InCollection)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSocketHelper::Compile);

		int32 NumOutPoints = 0;

		InputDataFacade = InDataFacade;

		for (const TPair<uint64, int32>& InInfos : InfosKeys)
		{
			const FSocketInfos& Infos = SocketInfosList[InInfos.Value];
			NumOutPoints += Infos.Count * Infos.Sockets.Num();
		}

		const int32 NumPoints = InDataFacade->GetNum(PCGExData::EIOSide::In);

		TSharedPtr<PCGExData::FPointIO> SocketIO = InCollection->Emplace_GetRef(InDataFacade->GetIn());
		SocketIO->IOIndex = InDataFacade->Source->IOIndex;

		PCGEX_INIT_IO_VOID(SocketIO, PCGExData::EIOInit::New)
		SocketFacade = MakeShared<PCGExData::FFacade>(SocketIO.ToSharedRef());

		UPCGBasePointData* OutPoints = SocketIO->GetOut();
		PCGEx::SetNumPointsAllocated(
			OutPoints, NumOutPoints,
			EPCGPointNativeProperties::MetadataEntry |
			EPCGPointNativeProperties::Transform |
			EPCGPointNativeProperties::Seed);

#define PCGEX_OUTPUT_INIT_LOCAL(_NAME, _TYPE, _DEFAULT_VALUE) if(Details->bWrite##_NAME){ _NAME##Writer = SocketFacade->GetWritable<_TYPE>(Details->_NAME##AttributeName, _DEFAULT_VALUE, true, PCGExData::EBufferInit::Inherit); }
		PCGEX_FOREACH_FIELD_SAMPLESOCKETS(PCGEX_OUTPUT_INIT_LOCAL)
#undef PCGEX_OUTPUT_INIT_LOCAL

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FSocketHelper::Compile::LoopPreparation);

			const UPCGMetadata* ParentMetadata = InputDataFacade->GetIn()->ConstMetadata();
			UPCGMetadata* Metadata = SocketFacade->GetOut()->MutableMetadata();
			Details->CarryOverDetails.Prune(Metadata);

			TConstPCGValueRange<int64> ReadMetadataEntry = InputDataFacade->GetIn()->GetConstMetadataEntryValueRange();
			TPCGValueRange<int64> OutMetadataEntry = SocketFacade->GetOut()->GetMetadataEntryValueRange();

			int32 WriteIndex = 0;
			for (int i = 0; i < NumPoints; i++)
			{
				int32 Idx = Mapping[i];
				if (Idx == -1) { continue; }

				StartIndices[i] = WriteIndex;

				const int32 NumSockets = SocketInfosList[Idx].Sockets.Num();
				const int64& InMetadataKey = ReadMetadataEntry[i];

				for (int j = 0; j < NumSockets; j++)
				{
					OutMetadataEntry[WriteIndex] = PCGInvalidEntryKey;
					Metadata->InitializeOnSet(OutMetadataEntry[WriteIndex], InMetadataKey, ParentMetadata);
					WriteIndex++;
				}
			}
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, CreateSocketPoints)
		CreateSocketPoints->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE, WeakManager = TWeakPtr<PCGExMT::FTaskManager>(AsyncManager)]()
			{
				PCGEX_ASYNC_THIS
				if (This->SocketNameWriter || This->SocketTagWriter || This->CategoryWriter || This->AssetPathWriter)
				{
					if (TSharedPtr<PCGExMT::FTaskManager> PinnedManager = WeakManager.Pin())
					{
						This->SocketFacade->WriteFastest(PinnedManager);
					}
				}
			};

		CreateSocketPoints->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->CompileRange(Scope);
			};

		CreateSocketPoints->StartSubLoops(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() * 4);
	}

	FSocketInfos& FSocketHelper::NewSocketInfos(const uint64 EntryHash, int32& OutIndex)
	{
		OutIndex = SocketInfosList.Num();
		InfosKeys.Add(EntryHash, OutIndex);
		return SocketInfosList.Emplace_GetRef();
	}

	void FSocketHelper::FilterSocketInfos(const int32 Index)
	{
		FSocketInfos& SocketInfos = SocketInfosList[Index];
		TArray<FPCGExSocket> ValidSockets;

		for (int i = 0; i < SocketInfos.Sockets.Num(); i++)
		{
			if (const FPCGExSocket& Socket = SocketInfos.Sockets[i];
				Details->SocketNameFilters.Test(Socket.SocketName.ToString()) &&
				Details->SocketTagFilters.Test(Socket.Tag))
			{
				ValidSockets.Add(Socket);
			}
		}

		SocketInfos.Sockets = ValidSockets;
	}

	void FSocketHelper::CompileRange(const PCGExMT::FScope& Scope)
	{
		const UPCGBasePointData* SourceData = InputDataFacade->Source->GetOutIn();

		TConstPCGValueRange<FTransform> ReadTransform = SourceData->GetConstTransformValueRange();
		TPCGValueRange<FTransform> OutTransform = SocketFacade->GetOut()->GetTransformValueRange();

		TPCGValueRange<int32> OutSeed = SocketFacade->GetOut()->GetSeedValueRange();


		PCGEX_SCOPE_LOOP(i)
		{
			int32 Index = StartIndices[i];
			if (Index == -1) { continue; }

			const FTransform& InTransform = ReadTransform[i];
			const FSocketInfos& SocketInfos = SocketInfosList[Mapping[i]];

			// Cache stable per-socketinfos values once
			const FName& Category = SocketInfos.Category;
			const FSoftObjectPath& Path = SocketInfos.Path;

			for (const FPCGExSocket& Socket : SocketInfos.Sockets)
			{
				FTransform WorldTransform = Socket.RelativeTransform * InTransform;
				const FVector WorldSc = WorldTransform.GetScale3D();
				FVector OutScale = Socket.RelativeTransform.GetScale3D();

				for (const int32 C : Details->TrScaComponents) { OutScale[C] = WorldSc[C]; }
				WorldTransform.SetScale3D(OutScale);

				OutTransform[Index] = WorldTransform;
				OutSeed[Index] = PCGExRandom::ComputeSpatialSeed(WorldTransform.GetLocation());

				PCGEX_OUTPUT_VALUE(SocketName, Index, Socket.SocketName)
				PCGEX_OUTPUT_VALUE(SocketTag, Index, FName(Socket.Tag))
				PCGEX_OUTPUT_VALUE(Category, Index, Category)
				PCGEX_OUTPUT_VALUE(AssetPath, Index, Path)

				Index++;
			}
		}
	}
}
