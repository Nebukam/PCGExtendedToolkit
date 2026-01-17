// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCage.h"

#include "EngineUtils.h"

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
	const int32 AssetCount = RegisteredAssets.Num();
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

void APCGExValencyCage::RegisterAsset(const TSoftObjectPtr<UObject>& Asset)
{
	if (Asset.IsNull())
	{
		return;
	}

	// Avoid duplicates
	for (const TSoftObjectPtr<UObject>& Existing : RegisteredAssets)
	{
		if (Existing == Asset)
		{
			return;
		}
	}

	RegisteredAssets.Add(Asset);
	OnAssetRegistrationChanged();
}

void APCGExValencyCage::UnregisterAsset(const TSoftObjectPtr<UObject>& Asset)
{
	const int32 Index = RegisteredAssets.IndexOfByKey(Asset);
	if (Index != INDEX_NONE)
	{
		RegisteredAssets.RemoveAt(Index);
		OnAssetRegistrationChanged();
	}
}

void APCGExValencyCage::ClearRegisteredAssets()
{
	if (RegisteredAssets.Num() > 0)
	{
		RegisteredAssets.Empty();
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

	// Scan for actors using virtual IsActorInside
	TArray<AActor*> ContainedActors;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this)
		{
			continue;
		}

		// Skip other cages
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
				RegisterAsset(TSoftObjectPtr<UObject>(Mesh));
			}
		}
		else if (UClass* ActorClass = Actor->GetClass())
		{
			// Check if it's a Blueprint
			if (UBlueprint* BP = Cast<UBlueprint>(ActorClass->ClassGeneratedBy))
			{
				RegisterAsset(TSoftObjectPtr<UObject>(BP));
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
