// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExPCGDataAssetCollection.h"

#include "PCGExGlobalSettings.h"
#include "Transform/PCGExTransform.h"

namespace PCGExPCGDataAssetCollection
{
	
	int32 FMicroCache::GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const
	{
		switch (PickMode)
		{
		default: case EPCGExIndexPickMode::Ascending: return GetPickAscending(Index);
		case EPCGExIndexPickMode::Descending: return GetPickDescending(Index);
		case EPCGExIndexPickMode::WeightAscending: return GetPickWeightAscending(Index);
		case EPCGExIndexPickMode::WeightDescending: return GetPickWeightDescending(Index);
		}
	}

	int32 FMicroCache::GetPickAscending(const int32 Index) const
	{
		return Order.IsValidIndex(Index) ? Index : -1;
	}

	int32 FMicroCache::GetPickDescending(const int32 Index) const
	{
		return Order.IsValidIndex(Index) ? (Order.Num() - 1) - Index : -1;
	}

	int32 FMicroCache::GetPickWeightAscending(const int32 Index) const
	{
		return Order.IsValidIndex(Index) ? Order[Index] : -1;
	}

	int32 FMicroCache::GetPickWeightDescending(const int32 Index) const
	{
		return Order.IsValidIndex(Index) ? Order[(Order.Num() - 1) - Index] : -1;
	}

	int32 FMicroCache::GetPickRandom(const int32 Seed) const
	{
		return Order[FRandomStream(Seed).RandRange(0, Order.Num() - 1)];
	}

	int32 FMicroCache::GetPickRandomWeighted(const int32 Seed) const
	{
		if (Order.IsEmpty()) { return -1; }

		const int32 Threshold = FRandomStream(Seed).RandRange(0, WeightSum - 1);
		int32 Pick = 0;
		while (Pick < Weights.Num() && Weights[Pick] < Threshold) { Pick++; }
		return Order[Pick];
	}
}

void FPCGExPCGDataAssetCollectionEntry::ClearSubCollection()
{
	FPCGExAssetCollectionEntry::ClearSubCollection();
	SubCollection = nullptr;
}

void FPCGExPCGDataAssetCollectionEntry::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const
{
	FPCGExAssetCollectionEntry::GetAssetPaths(OutPaths);
}


bool FPCGExPCGDataAssetCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (!bIsSubCollection)
	{
		if (!DataAsset.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }
	}

	return Super::Validate(ParentCollection);
}

UPCGExAssetCollection* FPCGExPCGDataAssetCollectionEntry::GetSubCollectionVoid() const
{
	return SubCollection;
}


#if WITH_EDITOR
void FPCGExPCGDataAssetCollectionEntry::EDITOR_Sanitize()
{
	FPCGExAssetCollectionEntry::EDITOR_Sanitize();

	if (!bIsSubCollection)
	{
		InternalSubCollection = nullptr;
	}
	else
	{
		InternalSubCollection = SubCollection;
	}
}
#endif

void FPCGExPCGDataAssetCollectionEntry::BuildMicroCache()
{
	const TSharedPtr<PCGExPCGDataAssetCollection::FMicroCache> NewCache = MakeShared<PCGExPCGDataAssetCollection::FMicroCache>();

	MicroCache = NewCache;
}

void FPCGExPCGDataAssetCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const int32 InInternalIndex, const bool bRecursive)
{
	ClearManagedSockets();

	if (bIsSubCollection)
	{
		Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
		return;
	}

	if (Staging.InternalIndex == -1 && GetDefault<UPCGExGlobalSettings>()->bDisableCollisionByDefault)
	{
	}

	Staging.Path = DataAsset.ToSoftObjectPath();

	TSharedPtr<FStreamableHandle> Handle = PCGExHelpers::LoadBlocking_AnyThread(DataAsset);

	const UPCGDataAsset* M = DataAsset.Get();

	if (M)
	{
		//update Staging
	}
	else
	{
		Staging.Bounds = FBox(ForceInit);
	}

	Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
	PCGExHelpers::SafeReleaseHandle(Handle);
}

void FPCGExPCGDataAssetCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	Super::SetAssetPath(InPath);
	DataAsset = TSoftObjectPtr<UPCGDataAsset>(InPath);
}

#if WITH_EDITOR
void UPCGExPCGDataAssetCollection::EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData)
{
	Super::EDITOR_AddBrowserSelectionInternal(InAssetData);

	for (const FAssetData& SelectedAsset : InAssetData)
	{
		TSoftObjectPtr<UPCGDataAsset> DataAsset = TSoftObjectPtr<UPCGDataAsset>(SelectedAsset.ToSoftObjectPath());
		if (!DataAsset.LoadSynchronous()) { continue; }

		bool bAlreadyExists = false;

		for (const FPCGExPCGDataAssetCollectionEntry& ExistingEntry : Entries)
		{
			if (ExistingEntry.DataAsset == DataAsset)
			{
				bAlreadyExists = true;
				break;
			}
		}

		if (bAlreadyExists) { continue; }

		FPCGExPCGDataAssetCollectionEntry Entry = FPCGExPCGDataAssetCollectionEntry();
		Entry.DataAsset = DataAsset;

		Entries.Add(Entry);
	}
}
#endif

void UPCGExPCGDataAssetCollection::EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const
{
	Super::EDITOR_RegisterTrackingKeys(Context);
	for (const FPCGExPCGDataAssetCollectionEntry& Entry : Entries)
	{
		if (Entry.bIsSubCollection && Entry.SubCollection)
		{
			Entry.SubCollection->EDITOR_RegisterTrackingKeys(Context);
		}
	}
}
