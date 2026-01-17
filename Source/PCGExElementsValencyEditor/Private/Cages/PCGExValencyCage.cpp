// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCage.h"

#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "PCGDataAsset.h"

APCGExValencyCage::APCGExValencyCage()
{
	// Standard cage setup
}

FString APCGExValencyCage::GetCageDisplayName() const
{
	// If we have a custom name, use it
	if (!CageName.IsEmpty())
	{
		return CageName;
	}

	// If we have registered assets, show count
	const int32 AssetCount = RegisteredAssetEntries.Num();
	if (AssetCount > 0)
	{
		return FString::Printf(TEXT("Cage [%d assets]"), AssetCount);
	}

	// If mirroring another cage
	if (MirrorSource.IsValid())
	{
		return FString::Printf(TEXT("Cage (Mirror: %s)"), *MirrorSource->GetCageDisplayName());
	}

	return TEXT("Cage (Empty)");
}

TArray<TSoftObjectPtr<UObject>> APCGExValencyCage::GetRegisteredAssets() const
{
	TArray<TSoftObjectPtr<UObject>> Assets;
	Assets.Reserve(RegisteredAssetEntries.Num());
	for (const FPCGExValencyAssetEntry& Entry : RegisteredAssetEntries)
	{
		if (Entry.IsValid())
		{
			Assets.Add(Entry.Asset);
		}
	}
	return Assets;
}

namespace
{
	EPCGExValencyAssetType DetectAssetType(const TSoftObjectPtr<UObject>& Asset)
	{
		if (Asset.IsNull())
		{
			return EPCGExValencyAssetType::Unknown;
		}

		// Try to load to check type
		if (UObject* LoadedAsset = Asset.LoadSynchronous())
		{
			if (LoadedAsset->IsA<UStaticMesh>())
			{
				return EPCGExValencyAssetType::Mesh;
			}
			if (LoadedAsset->IsA<UBlueprint>())
			{
				return EPCGExValencyAssetType::Actor;
			}
			if (LoadedAsset->IsA<UPCGDataAsset>())
			{
				return EPCGExValencyAssetType::DataAsset;
			}
		}

		// Fallback: check path for common patterns
		const FString Path = Asset.ToSoftObjectPath().ToString();
		if (Path.Contains(TEXT("/StaticMesh")) || Path.EndsWith(TEXT("_SM")))
		{
			return EPCGExValencyAssetType::Mesh;
		}

		return EPCGExValencyAssetType::Unknown;
	}
}

void APCGExValencyCage::RegisterAsset(const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor)
{
	if (Asset.IsNull())
	{
		return;
	}

	FPCGExValencyAssetEntry NewEntry;
	NewEntry.Asset = Asset;
	NewEntry.SourceActor = SourceActor;
	NewEntry.AssetType = DetectAssetType(Asset);

	// Compute local transform if we have a source actor and preservation is enabled
	if (bPreserveLocalTransforms && SourceActor)
	{
		const FTransform CageTransform = GetActorTransform();
		const FTransform ActorTransform = SourceActor->GetActorTransform();
		NewEntry.LocalTransform = ActorTransform.GetRelativeTransform(CageTransform);
	}

	// Check for duplicates - when preserving transforms, same asset with different transform is valid
	for (const FPCGExValencyAssetEntry& Existing : RegisteredAssetEntries)
	{
		if (Existing.Asset == Asset)
		{
			if (!bPreserveLocalTransforms)
			{
				// No transforms - simple duplicate check
				return;
			}
			// With transforms, check if transform is approximately the same
			if (Existing.LocalTransform.Equals(NewEntry.LocalTransform, 0.1f))
			{
				return;
			}
		}
	}

	RegisteredAssetEntries.Add(NewEntry);
	OnAssetRegistrationChanged();
}

void APCGExValencyCage::UnregisterAsset(const TSoftObjectPtr<UObject>& Asset)
{
	// Remove all entries with this asset
	const int32 RemovedCount = RegisteredAssetEntries.RemoveAll([&Asset](const FPCGExValencyAssetEntry& Entry)
	{
		return Entry.Asset == Asset;
	});

	if (RemovedCount > 0)
	{
		OnAssetRegistrationChanged();
	}
}

void APCGExValencyCage::ClearRegisteredAssets()
{
	if (RegisteredAssetEntries.Num() > 0)
	{
		RegisteredAssetEntries.Empty();
		OnAssetRegistrationChanged();
	}
}

void APCGExValencyCage::ScanAndRegisterContainedAssets()
{
	if (!bAutoRegisterContainedAssets)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Clear previous registrations before rescanning
	RegisteredAssetEntries.Empty();

	// Scan for actors using virtual IsActorInside
	TArray<AActor*> ContainedActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this)
		{
			continue;
		}

		// Skip other cages and volumes
		if (Cast<APCGExValencyCageBase>(Actor))
		{
			continue;
		}

		// Use virtual containment check
		if (IsActorInside(Actor))
		{
			ContainedActors.Add(Actor);
		}
	}

	// Also check child actors (always included regardless of bounds)
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);

	for (AActor* Child : ChildActors)
	{
		if (Child && !Cast<APCGExValencyCageBase>(Child))
		{
			ContainedActors.AddUnique(Child);
		}
	}

	// Register found actors
	for (AActor* Actor : ContainedActors)
	{
		// Try to get a meaningful asset reference
		// For static mesh actors, get the mesh
		// For blueprint actors, get the blueprint class
		// For others, just reference the actor's class

		if (const UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UStaticMesh* Mesh = SMC->GetStaticMesh())
			{
				RegisterAsset(TSoftObjectPtr<UObject>(Mesh), Actor);
			}
		}
		else if (UClass* ActorClass = Actor->GetClass())
		{
			// Check if it's a Blueprint
			if (UBlueprint* BP = Cast<UBlueprint>(ActorClass->ClassGeneratedBy))
			{
				RegisterAsset(TSoftObjectPtr<UObject>(BP), Actor);
			}
		}
	}
}

void APCGExValencyCage::OnAssetRegistrationChanged()
{
	// Notify containing volumes if auto-rebuild is enabled
	// TODO: Trigger rule rebuild

	// Mark as needing save
	Modify();
}
