// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExActorCollection.h"

bool FPCGExActorCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { LoadSubCollection(SubCollection); }
	else if (!Actor && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

void FPCGExActorCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive)
{
	if (bIsSubCollection)
	{
		Staging.Path = SubCollection.ToSoftObjectPath();
		if (bRecursive && SubCollection.LoadSynchronous()) { SubCollection.Get()->RebuildStagingData(true); }
		Super::UpdateStaging(OwningCollection, bRecursive);
		return;
	}

	Staging.Path = Actor.ToSoftObjectPath();
	const AActor* A = Actor.LoadSynchronous();

	PCGExAssetCollection::UpdateStagingBounds(Staging, A);

	Super::UpdateStaging(OwningCollection, bRecursive);
}

void FPCGExActorCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	Actor = TSoftObjectPtr<AActor>(InPath);
}

void FPCGExActorCollectionEntry::OnSubCollectionLoaded()
{
	SubCollectionPtr = Cast<UPCGExActorCollection>(BaseSubCollectionPtr);
}

#if WITH_EDITOR
void UPCGExActorCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExActorCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetAssetName() + TEXT("]")) : FName(Entry.Actor.GetAssetName());
	}
}
#endif

void UPCGExActorCollection::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const
{
	const bool bCollectionOnly = Flags == PCGExAssetCollection::ELoadingFlags::RecursiveCollectionsOnly;
	const bool bRecursive = bCollectionOnly || Flags == PCGExAssetCollection::ELoadingFlags::Recursive;

	for (const FPCGExActorCollectionEntry& Entry : Entries)
	{
		if (Entry.bIsSubCollection)
		{
			if (bRecursive || bCollectionOnly)
			{
				if (const UPCGExActorCollection* SubCollection = Entry.SubCollection.LoadSynchronous())
				{
					SubCollection->GetAssetPaths(OutPaths, Flags);
				}
			}
			continue;
		}

		if (bCollectionOnly) { continue; }
		if (!Entry.Actor.Get()) { OutPaths.Add(Entry.Actor.ToSoftObjectPath()); }
	}
}
