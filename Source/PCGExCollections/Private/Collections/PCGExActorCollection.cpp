// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExActorCollection.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "Engine/Blueprint.h"

// Register the Actor collection type at startup
PCGEX_REGISTER_COLLECTION_TYPE(Actor, UPCGExActorCollection, FPCGExActorCollectionEntry, "Actor Collection", Base)

#pragma region FPCGExActorCollectionEntry

const UPCGExAssetCollection* FPCGExActorCollectionEntry::GetSubCollectionPtr() const
{
	return SubCollection;
}

void FPCGExActorCollectionEntry::ClearSubCollection()
{
	FPCGExAssetCollectionEntry::ClearSubCollection();
	SubCollection = nullptr;
}

bool FPCGExActorCollectionEntry::Validate(const UPCGExAssetCollection* ParentCollection)
{
	if (!bIsSubCollection)
	{
		if (!Actor.ToSoftObjectPath().IsValid() && ParentCollection->bDoNotIgnoreInvalidEntries) { return false; }
	}

	return FPCGExAssetCollectionEntry::Validate(ParentCollection);
}

void FPCGExActorCollectionEntry::UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, bool bRecursive)
{
	ClearManagedSockets();

	if (bIsSubCollection)
	{
		FPCGExAssetCollectionEntry::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
		return;
	}

	Staging.Path = Actor.ToSoftObjectPath();

	// Load the actor class to compute bounds
	TSharedPtr<FStreamableHandle> Handle = PCGExHelpers::LoadBlocking_AnyThread(Actor.ToSoftObjectPath());

	if (const UClass* ActorClass = Actor.Get())
	{
		// Try to get default object bounds
		if (const AActor* CDO = Cast<AActor>(ActorClass->GetDefaultObject()))
		{
			FVector Origin, BoxExtent;
			CDO->GetActorBounds(false, Origin, BoxExtent);
			Staging.Bounds = FBox(Origin - BoxExtent, Origin + BoxExtent);
		}
		else
		{
			Staging.Bounds = FBox(ForceInit);
		}
	}
	else
	{
		Staging.Bounds = FBox(ForceInit);
	}

	FPCGExAssetCollectionEntry::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
	PCGExHelpers::SafeReleaseHandle(Handle);
}

void FPCGExActorCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	FPCGExAssetCollectionEntry::SetAssetPath(InPath);
	Actor = TSoftClassPtr<AActor>(InPath);
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

#pragma endregion

#if WITH_EDITOR
void UPCGExActorCollection::EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData)
{
	UPCGExAssetCollection::EDITOR_AddBrowserSelectionInternal(InAssetData);

	for (const FAssetData& SelectedAsset : InAssetData)
	{
		// Handle Blueprint assets
		if (SelectedAsset.AssetClassPath == UBlueprint::StaticClass()->GetClassPathName())
		{
			const UBlueprint* Blueprint = Cast<UBlueprint>(SelectedAsset.GetAsset());
			if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
			{
				continue;
			}

			TSoftClassPtr<AActor> ActorClass = TSoftClassPtr<AActor>(Blueprint->GeneratedClass.Get());

			bool bAlreadyExists = false;
			for (const FPCGExActorCollectionEntry& ExistingEntry : Entries)
			{
				if (ExistingEntry.Actor == ActorClass)
				{
					bAlreadyExists = true;
					break;
				}
			}

			if (bAlreadyExists) { continue; }

			FPCGExActorCollectionEntry Entry = FPCGExActorCollectionEntry();
			Entry.Actor = ActorClass;

			Entries.Add(Entry);
		}
	}
}
#endif
