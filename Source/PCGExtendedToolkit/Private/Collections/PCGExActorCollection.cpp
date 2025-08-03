// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExActorCollection.h"
#include "Engine/Blueprint.h"

void FPCGExActorCollectionEntry::GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const
{
	// This is a subclass, no asset to load.
	//FPCGExAssetCollectionEntry::GetAssetPaths(OutPaths);
}

bool FPCGExActorCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (bIsSubCollection) { return SubCollection ? true : false; }
	if (!Actor && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }

	return Super::Validate(ParentCollection);
}

#if WITH_EDITOR
void FPCGExActorCollectionEntry::EDITOR_Sanitize()
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

void FPCGExActorCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, const int32 InInternalIndex, const bool bRecursive)
{
	ClearManagedSockets();
	
	if (bIsSubCollection)
	{
		Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
		return;
	}
	
	Staging.Path = Actor ? Actor->GetPathName() : FSoftObjectPath();

	// TODO : Implement socket from tagged components
	
	PCGExAssetCollection::UpdateStagingBounds(Staging, Actor, bOnlyCollidingComponents, bIncludeFromChildActors);

	Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
}

void FPCGExActorCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	Super::SetAssetPath(InPath);
	Actor = TSoftClassPtr<AActor>(InPath);
}

#if WITH_EDITOR
void UPCGExActorCollection::EDITOR_RefreshDisplayNames()
{
	Super::EDITOR_RefreshDisplayNames();
	for (FPCGExActorCollectionEntry& Entry : Entries)
	{
		Entry.DisplayName = Entry.bIsSubCollection ? FName(TEXT("[") + Entry.SubCollection.GetName() + TEXT("]")) : FName(Entry.Actor ? Entry.Actor->GetName() : TEXT("None"));
	}
}

void UPCGExActorCollection::EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData)
{
	Super::EDITOR_AddBrowserSelectionInternal(InAssetData);

	static const FName GeneratedClassTag = "GeneratedClass";

	for (const FAssetData& SelectedAsset : InAssetData)
	{
		TSoftClassPtr<AActor> Actor = nullptr;

		// Ensure the asset is a Blueprint
		if (SelectedAsset.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
		{
			if (FString ClassPath;
				SelectedAsset.GetTagValue(GeneratedClassTag, ClassPath))
			{
				Actor = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath));
			}
			else { continue; }
		}
		else if (SelectedAsset.AssetClassPath == UClass::StaticClass()->GetClassPathName())
		{
			Actor = TSoftClassPtr<AActor>(SelectedAsset.ToSoftObjectPath());
		}

		if (!Actor.LoadSynchronous()) { continue; }

		bool bAlreadyExists = false;

		for (const FPCGExActorCollectionEntry& ExistingEntry : Entries)
		{
			if (ExistingEntry.Actor == Actor)
			{
				bAlreadyExists = true;
				break;
			}
		}

		if (bAlreadyExists) { continue; }

		FPCGExActorCollectionEntry Entry = FPCGExActorCollectionEntry();
		Entry.Actor = Actor;

		Entries.Add(Entry);
	}
}
#endif
