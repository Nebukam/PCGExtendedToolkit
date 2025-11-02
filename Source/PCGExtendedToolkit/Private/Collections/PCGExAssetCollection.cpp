// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExAssetCollection.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif

#include "PCGEx.h"
#include "Details/PCGExMacros.h"
#include "Details/PCGExDetailsSettings.h"

namespace PCGExAssetCollection
{
	int32 FCategory::GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const
	{
		switch (PickMode)
		{
		default:
		case EPCGExIndexPickMode::Ascending:
			return GetPickAscending(Index);
		case EPCGExIndexPickMode::Descending:
			return GetPickDescending(Index);
		case EPCGExIndexPickMode::WeightAscending:
			return GetPickWeightAscending(Index);
		case EPCGExIndexPickMode::WeightDescending:
			return GetPickWeightDescending(Index);
		}
	}

	int32 FCategory::GetPickAscending(const int32 Index) const
	{
		return Indices.IsValidIndex(Index) ? Indices[Index] : -1;
	}

	int32 FCategory::GetPickDescending(const int32 Index) const
	{
		return Indices.IsValidIndex(Index) ? Indices[(Indices.Num() - 1) - Index] : -1;
	}

	int32 FCategory::GetPickWeightAscending(const int32 Index) const
	{
		return Order.IsValidIndex(Index) ? Indices[Order[Index]] : -1;
	}

	int32 FCategory::GetPickWeightDescending(const int32 Index) const
	{
		return Order.IsValidIndex(Index) ? Indices[Order[(Order.Num() - 1) - Index]] : -1;
	}

	int32 FCategory::GetPickRandom(const int32 Seed) const
	{
		if (Order.IsEmpty()) { return -1; }
		return Indices[Order[FRandomStream(Seed).RandRange(0, Order.Num() - 1)]];
	}

	int32 FCategory::GetPickRandomWeighted(const int32 Seed) const
	{
		if (Order.IsEmpty()) { return -1; }
		const int32 Threshold = FRandomStream(Seed).RandRange(0, WeightSum - 1);
		int32 Pick = 0;
		while (Pick < Weights.Num() && Weights[Pick] < Threshold) { Pick++; }
		return Indices[Order[Pick]];
	}

	void FCategory::Reserve(const int32 Num)
	{
		Indices.Reserve(Num);
		Weights.Reserve(Num);
		Order.Reserve(Num);
	}

	void FCategory::Shrink()
	{
		Indices.Shrink();
		Weights.Shrink();
		Order.Shrink();
	}

	void FCategory::RegisterEntry(const int32 Index, const FPCGExAssetCollectionEntry* InEntry)
	{
		Entries.Add(InEntry);
		const_cast<FPCGExAssetCollectionEntry*>(InEntry)->BuildMicroCache(); // Dealing with legacy shit implementation
		Indices.Add(Index);
		Weights.Add(InEntry->Weight + 1);
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

bool FPCGExSocketOutputDetails::Init(FPCGExContext* InContext)
{
	if (bWriteSocketName) { PCGEX_VALIDATE_NAME_C(InContext, SocketNameAttributeName) }
	if (bWriteSocketTag) { PCGEX_VALIDATE_NAME_C(InContext, SocketTagAttributeName) }
	if (bWriteCategory) { PCGEX_VALIDATE_NAME_C(InContext, CategoryAttributeName) }
	if (bWriteAssetPath) { PCGEX_VALIDATE_NAME_C(InContext, AssetPathAttributeName) }

	SocketTagFilters.Init();
	SocketNameFilters.Init();
	CarryOverDetails.Init();

#define PCGEX_REGISTER_FLAG(_COMPONENT, _ARRAY) \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::X)) != 0){ _ARRAY.Add(0); } \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Y)) != 0){ _ARRAY.Add(1); } \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Z)) != 0){ _ARRAY.Add(2); }

	PCGEX_REGISTER_FLAG(TransformScale, TrScaComponents)

#undef PCGEX_REGISTER_FLAG

	return true;
}

bool FPCGExAssetStagingData::FindSocket(const FName InName, const FPCGExSocket*& OutSocket) const
{
	for (const FPCGExSocket& Socket : Sockets)
	{
		if (Socket.SocketName == InName)
		{
			OutSocket = &Socket;
			return true;
		}
	}
	return false;
}

bool FPCGExAssetStagingData::FindSocket(const FName InName, const FString& Tag, const FPCGExSocket*& OutSocket) const
{
	for (const FPCGExSocket& Socket : Sockets)
	{
		if (Socket.SocketName == InName && Socket.Tag == Tag)
		{
			OutSocket = &Socket;
			return true;
		}
	}
	return false;
}

#if WITH_EDITOR

void FPCGExAssetCollectionEntry::EDITOR_Sanitize()
{
}
#endif

bool FPCGExAssetCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	// Skip entries with a weight of 0
	if (Weight <= 0) { return false; }

	if (bIsSubCollection)
	{
		if (!InternalSubCollection) { return false; }
		InternalSubCollection->LoadCache();
	}
	return true;
}

void FPCGExAssetCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const int32 InInternalIndex, const bool bRecursive)
{
	Staging.InternalIndex = InInternalIndex;

	if (bIsSubCollection)
	{
		Staging.Bounds = FBox(ForceInit);
		if (InternalSubCollection)
		{
			Staging.Path = FSoftObjectPath(InternalSubCollection.GetPathName());
			if (bRecursive) { InternalSubCollection->RebuildStagingData(true); }
		}
		else
		{
			Staging.Path = FSoftObjectPath{};
		}
	}
}

void FPCGExAssetCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	Staging.Path = InPath;
}

void FPCGExAssetCollectionEntry::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const
{
	OutPaths.Emplace(Staging.Path);
}

void FPCGExAssetCollectionEntry::BuildMicroCache()
{
	MicroCache = nullptr;
}

void FPCGExAssetCollectionEntry::ClearManagedSockets()
{
	int32 WriteIndex = 0;
	for (int i = 0; i < Staging.Sockets.Num(); i++) { if (!Staging.Sockets[i].bManaged) { Staging.Sockets[WriteIndex++] = Staging.Sockets[i]; } }
	Staging.Sockets.SetNum(WriteIndex);
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
			PCGEX_MAKE_SHARED(Category, FCategory, InEntry->Category)
			Categories.Add(InEntry->Category, Category);
			Category->RegisterEntry(Index, InEntry);
		}
		else
		{
			(*CategoryPtr)->RegisterEntry(Index, InEntry);
		}
	}

	void GetBoundingBoxBySpawning(const TSoftClassPtr<AActor>& InActorClass, FVector& Origin, FVector& BoxExtent, const bool bOnlyCollidingComponents, const bool bIncludeFromChildActors)
	{
#if WITH_EDITOR
		UWorld* World = GWorld;
		if (!World)
		{
			UE_LOG(LogPCGEx, Error, TEXT("No world to compute actor bounds!"));
			return;
		}

		if (IsInGameThread())
		{
			UClass* ActorClass = InActorClass.LoadSynchronous();
			if (!ActorClass) { return; }

			FActorSpawnParameters SpawnParams;
			SpawnParams.bNoFail = true;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* TempActor = World->SpawnActor<AActor>(ActorClass, FTransform(), SpawnParams);
			if (!TempActor)
			{
				UE_LOG(LogPCGEx, Error, TEXT("Failed to create temp actor!"));
				return;
			}

			// Compute the bounds
			TempActor->GetActorBounds(bOnlyCollidingComponents, Origin, BoxExtent, bIncludeFromChildActors);

			// Hide the actor to ensure it doesn't affect gameplay or rendering
			TempActor->SetActorHiddenInGame(true);
			TempActor->SetActorEnableCollision(false);

			// Destroy the temporary actor
			TempActor->Destroy();
		}
		else
		{
			// If this throw, it's because a collection has been initialized outside of game thread, which is bad.
			UE_LOG(LogPCGEx, Error, TEXT("GetBoundingBoxBySpawning executed outside of game thread."));
		}
#else
		UE_LOG(LogPCGEx, Error, TEXT("GetBoundingBoxBySpawning called in non-editor context."));
#endif
	}

	void UpdateStagingBounds(FPCGExAssetStagingData& InStaging, const TSoftClassPtr<AActor>& InActor, const bool bOnlyCollidingComponents, const bool bIncludeFromChildActors)
	{
		FVector Origin = FVector::ZeroVector;
		FVector Extents = FVector::ZeroVector;
		GetBoundingBoxBySpawning(InActor, Origin, Extents, bOnlyCollidingComponents, bIncludeFromChildActors);

		InStaging.Bounds = FBoxCenterAndExtent(Origin, Extents).GetBox();
	}

	void UpdateStagingBounds(FPCGExAssetStagingData& InStaging, const UStaticMesh* InMesh)
	{
		if (!InMesh)
		{
			InStaging.Bounds = FBox(ForceInit);
			return;
		}

		InStaging.Bounds = InMesh->GetBoundingBox();
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

void UPCGExAssetCollection::EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const
{
	Context->EDITOR_TrackPath(this);
}

#if WITH_EDITOR
void UPCGExAssetCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property) { Super::PostEditChangeProperty(PropertyChangedEvent); }

	EDITOR_RefreshDisplayNames();
	EDITOR_SetDirty();

	if (bAutoRebuildStaging) { EDITOR_RebuildStagingData(); }
}

void UPCGExAssetCollection::EDITOR_RefreshDisplayNames()
{
}

void UPCGExAssetCollection::EDITOR_AddBrowserSelection()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FAssetData> SelectedAssets;
	ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.IsEmpty()) { return; }

	EDITOR_AddBrowserSelectionTyped(SelectedAssets);
}

void UPCGExAssetCollection::EDITOR_AddBrowserSelectionTyped(const TArray<FAssetData>& InAssetData)
{
	Modify(true);
	EDITOR_AddBrowserSelectionInternal(InAssetData);
	EDITOR_RefreshDisplayNames();
	MarkPackageDirty();
	FCoreUObjectDelegates::BroadcastOnObjectModified(this);
	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData()
{
	Modify(true);
	InvalidateCache();
	EDITOR_SanitizeAndRebuildStagingData(false);
	MarkPackageDirty();
	FCoreUObjectDelegates::BroadcastOnObjectModified(this);
	//CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void UPCGExAssetCollection::EDITOR_RebuildStagingData_Recursive()
{
	Modify(true);
	InvalidateCache();
	EDITOR_SanitizeAndRebuildStagingData(true);
	MarkPackageDirty();
	FCoreUObjectDelegates::BroadcastOnObjectModified(this);
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
EDITOR_RefreshDisplayNames(); \
FPropertyChangedEvent EmptyEvent(nullptr); \
PostEditChangeProperty(EmptyEvent); \
MarkPackageDirty(); \
FCoreUObjectDelegates::BroadcastOnObjectModified(this);

void UPCGExAssetCollection::EDITOR_SortByWeightAscending() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_SortByWeightAscendingTyped();) }

void UPCGExAssetCollection::EDITOR_SortByWeightAscendingTyped()
{
}

void UPCGExAssetCollection::EDITOR_SortByWeightDescending() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_SortByWeightDescendingTyped();) }

void UPCGExAssetCollection::EDITOR_SortByWeightDescendingTyped()
{
}

void UPCGExAssetCollection::EDITOR_SetWeightIndex() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_SetWeightIndexTyped();) }

void UPCGExAssetCollection::EDITOR_SetWeightIndexTyped()
{
}

void UPCGExAssetCollection::EDITOR_PadWeight() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_PadWeightTyped();) }

void UPCGExAssetCollection::EDITOR_PadWeightTyped()
{
}

void UPCGExAssetCollection::EDITOR_MultWeight2() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_MultWeight2Typed();) }

void UPCGExAssetCollection::EDITOR_MultWeight2Typed()
{
}

void UPCGExAssetCollection::EDITOR_MultWeight10() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_MultWeight10Typed();) }

void UPCGExAssetCollection::EDITOR_MultWeight10Typed()
{
}

void UPCGExAssetCollection::EDITOR_WeightOne() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_WeightOneTyped();) }

void UPCGExAssetCollection::EDITOR_WeightOneTyped()
{
}

void UPCGExAssetCollection::EDITOR_WeightRandom() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_WeightRandomTyped();) }

void UPCGExAssetCollection::EDITOR_WeightRandomTyped()
{
}

void UPCGExAssetCollection::EDITOR_NormalizedWeightToSum() { PCGEX_ASSET_COLLECTION_UTIL_CALL(EDITOR_NormalizedWeightToSumTyped();); }

void UPCGExAssetCollection::EDITOR_NormalizedWeightToSumTyped()
{
}

void UPCGExAssetCollection::EDITOR_SanitizeAndRebuildStagingData(const bool bRecursive)
{
}

void UPCGExAssetCollection::EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData)
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


FPCGExRoamingAssetCollectionDetails::FPCGExRoamingAssetCollectionDetails(const TSubclassOf<UPCGExAssetCollection>& InAssetCollectionType)
	: bSupportCustomType(false), AssetCollectionType(InAssetCollectionType)
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
	UPCGExAssetCollection* Collection = InContext->ManagedObjects->New<UPCGExAssetCollection>(GetTransientPackage(), AssetCollectionType.Get(), NAME_None);
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
	UPCGExAssetCollection* Collection = InContext->ManagedObjects->New<UPCGExAssetCollection>(GetTransientPackage(), AssetCollectionType.Get(), NAME_None);
	if (!Collection) { return nullptr; }

	if (!Collection->BuildFromAttributeSet(InContext, InputPin, *this, bBuildStaging))
	{
		InContext->ManagedObjects->Destroy(Collection);
		return nullptr;
	}

	return Collection;
}
