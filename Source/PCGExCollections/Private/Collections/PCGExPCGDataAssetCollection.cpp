// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExPCGDataAssetCollection.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGDataAsset.h"
#include "Data/PCGSpatialData.h"


// Register the PCGDataAsset collection type at startup
PCGEX_REGISTER_COLLECTION_TYPE(PCGDataAsset, UPCGExPCGDataAssetCollection, FPCGExPCGDataAssetCollectionEntry, "PCG Data Asset Collection", Base)

// PCGDataAsset MicroCache - Point weight picking

namespace PCGExPCGDataAssetCollection
{
	void FMicroCache::ProcessPointWeights(const TArray<int32>& InPointWeights)
	{
		BuildFromWeights(InPointWeights);
	}
}

// PCGDataAsset Collection Entry

UPCGExAssetCollection* FPCGExPCGDataAssetCollectionEntry::GetSubCollectionPtr() const
{
	return SubCollection;
}

void FPCGExPCGDataAssetCollectionEntry::ClearSubCollection()
{
	FPCGExAssetCollectionEntry::ClearSubCollection();
	SubCollection = nullptr;
}

bool FPCGExPCGDataAssetCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (!bIsSubCollection)
	{
		if (!DataAsset.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }
	}

	return FPCGExAssetCollectionEntry::Validate(ParentCollection);
}

void FPCGExPCGDataAssetCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, bool bRecursive)
{
	ClearManagedSockets();

	if (bIsSubCollection)
	{
		FPCGExAssetCollectionEntry::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
		return;
	}

	Staging.Path = DataAsset.ToSoftObjectPath();

	// Load the data asset to compute bounds
	TSharedPtr<FStreamableHandle> Handle = PCGExHelpers::LoadBlocking_AnyThreadTpl(DataAsset);

	if (const UPCGDataAsset* Asset = DataAsset.Get())
	{
		// Compute bounds from all point data in the asset
		FBox CombinedBounds(ForceInit);

		for (const FPCGTaggedData& TaggedData : Asset->Data.GetAllInputs())
		{
			if (const UPCGSpatialData* PointData = Cast<UPCGSpatialData>(TaggedData.Data))
			{
				CombinedBounds += PointData->GetBounds();
			}
		}

		Staging.Bounds = CombinedBounds.IsValid ? CombinedBounds : FBox(ForceInit);
	}
	else
	{
		Staging.Bounds = FBox(ForceInit);
	}

	FPCGExAssetCollectionEntry::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
	PCGExHelpers::SafeReleaseHandle(Handle);
}

void FPCGExPCGDataAssetCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	FPCGExAssetCollectionEntry::SetAssetPath(InPath);
	DataAsset = TSoftObjectPtr<UPCGDataAsset>(InPath);
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
	if (!bOverrideWeights || PointWeights.IsEmpty())
	{
		MicroCache = nullptr;
		return;
	}

	TSharedPtr<PCGExPCGDataAssetCollection::FMicroCache> NewCache = MakeShared<PCGExPCGDataAssetCollection::FMicroCache>();
	NewCache->ProcessPointWeights(PointWeights);
	MicroCache = NewCache;
}


#if WITH_EDITOR

// PCGDataAsset Collection - Editor Functions

void UPCGExPCGDataAssetCollection::EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData)
{
	UPCGExAssetCollection::EDITOR_AddBrowserSelectionInternal(InAssetData);

	for (const FAssetData& SelectedAsset : InAssetData)
	{
		TSoftObjectPtr<UPCGDataAsset> Asset = TSoftObjectPtr<UPCGDataAsset>(SelectedAsset.ToSoftObjectPath());
		if (!Asset.LoadSynchronous()) { continue; }

		bool bAlreadyExists = false;
		for (const FPCGExPCGDataAssetCollectionEntry& ExistingEntry : Entries)
		{
			if (ExistingEntry.DataAsset == Asset)
			{
				bAlreadyExists = true;
				break;
			}
		}

		if (bAlreadyExists) { continue; }

		FPCGExPCGDataAssetCollectionEntry Entry = FPCGExPCGDataAssetCollectionEntry();
		Entry.DataAsset = Asset;

		Entries.Add(Entry);
	}
}
#endif
