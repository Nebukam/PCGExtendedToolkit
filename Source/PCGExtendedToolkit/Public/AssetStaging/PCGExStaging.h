﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Details/PCGExDetailsStaging.h"
#include "Transform/PCGExTransform.h"

namespace PCGExAssetCollection
{
	class FCache;
}

struct FPCGExActorCollectionEntry;
struct FPCGExAssetCollectionEntry;
struct FPCGMeshInstanceList;
struct FPCGContext;
class UPCGBasePointData;
class UPCGParamData;
struct FPCGExContext;
struct FPCGExMeshCollectionEntry;
class UPCGExAssetCollection;
class UPCGExMeshCollection;
class UPCGExActorCollection;

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}


UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Component Flags"))
enum class EPCGExAbsoluteRotationFlags : uint8
{
	None = 0,
	X    = 1 << 0 UMETA(DisplayName = "Pitch"),
	Y    = 1 << 1 UMETA(DisplayName = "Yaw"),
	Z    = 1 << 2 UMETA(DisplayName = "Roll"),
	All  = X | Y | Z UMETA(DisplayName = "All"),
};

ENUM_CLASS_FLAGS(EPCGExAbsoluteRotationFlags)
using EPCGExAbsoluteRotationFlagsBitmask = TEnumAsByte<EPCGExAbsoluteRotationFlags>;

namespace PCGExStaging
{
	const FName SourceCollectionMapLabel = TEXT("Map");
	const FName OutputCollectionMapLabel = TEXT("Map");
	const FName OutputSocketLabel = TEXT("Sockets");

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

	template <typename C, typename A>
	class TPickUnpacker : public IPickUnpacker
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

	template <typename C, typename A>
	class TDistributionHelper : public IDistributionHelper
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
		FSoftObjectPath Path;
		FName Category = NAME_None;
		TArray<FPCGExSocket> Sockets;
		int32 Count = 0;
	};

#define PCGEX_FOREACH_FIELD_SAMPLESOCKETS(MACRO)\
MACRO(SocketName, FName, NAME_None) \
MACRO(SocketTag, FName, NAME_None) \
MACRO(Category, FName, NAME_None) \
MACRO(AssetPath, FSoftObjectPath, FSoftObjectPath{})

	PCGEXTENDEDTOOLKIT_API
	uint64 GetSimplifiedEntryHash(uint64 InEntryHash);

	class PCGEXTENDEDTOOLKIT_API FSocketHelper : public TSharedFromThis<FSocketHelper>
	{
		FRWLock SocketLock;
		const FPCGExSocketOutputDetails* Details = nullptr;

		TMap<uint64, int32> InfosKeys;
		TArray<FSocketInfos> SocketInfosList;
		TArray<int32> Mapping;
		TArray<int32> StartIndices;

		PCGEX_FOREACH_FIELD_SAMPLESOCKETS(PCGEX_OUTPUT_DECL)

	public:
		explicit FSocketHelper(const FPCGExSocketOutputDetails* InDetails, const int32 InNumPoints);

		TSharedPtr<PCGExData::FFacade> InputDataFacade;
		TSharedPtr<PCGExData::FFacade> SocketFacade;

		void Add(const int32 Index, const uint64 EntryHash, const FPCGExAssetCollectionEntry* Entry);
		void Add(const int32 Index, const TObjectPtr<UStaticMesh>& Mesh);

		void Compile(
			const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
			const TSharedPtr<PCGExData::FFacade>& InDataFacade,
			const TSharedPtr<PCGExData::FPointIOCollection>& InCollection);

	protected:
		FSocketInfos& NewSocketInfos(const uint64 EntryHash, int32& OutIndex);
		void FilterSocketInfos(const int32 Index);
		void CompileRange(const PCGExMT::FScope& Scope);
	};
}
