// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExActorCollection.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGExtendedToolkit.h"
#include "Engine/Blueprint.h"

namespace PCGExActorCollection
{
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
}

void FPCGExActorCollectionEntry::ClearSubCollection()
{
	FPCGExAssetCollectionEntry::ClearSubCollection();
	SubCollection = nullptr;
}

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

	FVector Origin = FVector::ZeroVector;
	FVector Extents = FVector::ZeroVector;

	PCGExActorCollection::GetBoundingBoxBySpawning(Actor, Origin, Extents, bOnlyCollidingComponents, bIncludeFromChildActors);
	Staging.Bounds = FBoxCenterAndExtent(Origin, Extents).GetBox();

	Super::UpdateStaging(OwningCollection, InInternalIndex, bRecursive);
}

void FPCGExActorCollectionEntry::SetAssetPath(const FSoftObjectPath& InPath)
{
	Super::SetAssetPath(InPath);
	Actor = TSoftClassPtr<AActor>(InPath);
}

UPCGExAssetCollection* FPCGExActorCollectionEntry::GetSubCollectionVoid() const
{
	return SubCollection;
}

#if WITH_EDITOR
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
			if (FString ClassPath; SelectedAsset.GetTagValue(GeneratedClassTag, ClassPath))
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
