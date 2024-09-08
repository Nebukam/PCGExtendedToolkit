// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExInternalCollection.h"

#include "Collections/PCGExActorCollection.h"
#include "Engine/AssetManager.h"

bool FPCGExInternalCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!Object.IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

void FPCGExInternalCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		Staging.Path = SubCollection.ToSoftObjectPath();
		if (bRecursive && SubCollection.LoadSynchronous())
		{
			if (UPCGExInternalCollection* InternalCollection = Cast<UPCGExInternalCollection>(SubCollection.Get()))
			{
				// Only rebuild if Internal
				InternalCollection->RebuildStagingData(true);
			}
		}
		Super::UpdateStaging(OwningCollection, bRecursive);
		return;
	}

	Staging.Path = Object;

	// TODO : Refactor this. We can load all from the collection itself instead
	// better yet, from Context' data preparation

	UObject* LoadedAsset = UAssetManager::GetStreamableManager().RequestSyncLoad(Object)->GetLoadedAsset();

	Staging.Bounds = FBox(ForceInitToZero);

	if (const UStaticMesh* Mesh = Cast<UStaticMesh>(LoadedAsset)) { PCGExAssetCollection::UpdateStagingBounds(Staging, Mesh); }
	else if (const AActor* Actor = Cast<AActor>(LoadedAsset)) { PCGExAssetCollection::UpdateStagingBounds(Staging, Actor); }

	Super::UpdateStaging(OwningCollection, bRecursive);
}

void FPCGExInternalCollectionEntry::SetAssetPath(FSoftObjectPath InPath)
{
	Object = InPath;
}

void FPCGExInternalCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExInternalCollection>(BaseSubCollectionPtr);
}

void UPCGExInternalCollection::RebuildStagingData(const bool bRecursive)
{
	for (FPCGExInternalCollectionEntry& Entry : Entries) { Entry.UpdateStaging(this, bRecursive); }
	Super::RebuildStagingData(bRecursive);
}

UPCGExAssetCollection* UPCGExInternalCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExInternalCollection>(InContext, InAttributeSet, Details, bBuildStaging);
}

UPCGExAssetCollection* UPCGExInternalCollection::GetCollectionFromAttributeSet(const FPCGContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const
{
	return GetCollectionFromAttributeSetTpl<UPCGExInternalCollection>(InContext, InputPin, Details, bBuildStaging);
}

void UPCGExInternalCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const
{
	const bool bCollectionOnly = Flags == PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly;
	const bool bRecursive = bCollectionOnly || Flags == PCGExAssetCollection::ELoadingFlags::Recursive;

	for (const FPCGExInternalCollectionEntry& Entry : Entries)
	{
		if (Entry.bIsSubCollection)
		{
			if (bRecursive || bCollectionOnly)
			{
				if (const UPCGExInternalCollection* SubCollection = Entry.SubCollection.LoadSynchronous())
				{
					SubCollection->GetAssetPaths(OutPaths, Flags);
				}
			}
			continue;
		}

		if (bCollectionOnly) { continue; }

		if (!Entry.Object.ResolveObject()) { OutPaths.Add(Entry.Object); }
	}
}

void UPCGExInternalCollection::BuildCache()
{
	Super::BuildCache(Entries);
}
