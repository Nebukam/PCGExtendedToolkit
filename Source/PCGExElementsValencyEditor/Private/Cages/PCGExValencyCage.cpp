// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCage.h"

#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "PCGDataAsset.h"
#include "Components/StaticMeshComponent.h"

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
	const int32 ManualCount = ManualAssetEntries.Num();
	const int32 ScannedCount = ScannedAssetEntries.Num();
	const int32 TotalCount = ManualCount + ScannedCount;
	if (TotalCount > 0)
	{
		if (ManualCount > 0 && ScannedCount > 0)
		{
			return FString::Printf(TEXT("Cage [%d+%d assets]"), ManualCount, ScannedCount);
		}
		return FString::Printf(TEXT("Cage [%d assets]"), TotalCount);
	}

	// If mirroring another cage
	if (MirrorSource.IsValid())
	{
		return FString::Printf(TEXT("Cage (Mirror: %s)"), *MirrorSource->GetCageDisplayName());
	}

	return TEXT("Cage (Empty)");
}

TArray<FPCGExValencyAssetEntry> APCGExValencyCage::GetAllAssetEntries() const
{
	TArray<FPCGExValencyAssetEntry> AllEntries;
	AllEntries.Reserve(ManualAssetEntries.Num() + ScannedAssetEntries.Num());

	// Manual entries first (user-defined priority)
	AllEntries.Append(ManualAssetEntries);

	// Then scanned entries (skip duplicates of manual)
	for (const FPCGExValencyAssetEntry& ScannedEntry : ScannedAssetEntries)
	{
		bool bIsDuplicate = false;
		for (const FPCGExValencyAssetEntry& ManualEntry : ManualAssetEntries)
		{
			if (ManualEntry.Asset == ScannedEntry.Asset)
			{
				bIsDuplicate = true;
				break;
			}
		}
		if (!bIsDuplicate)
		{
			AllEntries.Add(ScannedEntry);
		}
	}

	return AllEntries;
}

TArray<TSoftObjectPtr<UObject>> APCGExValencyCage::GetRegisteredAssets() const
{
	const TArray<FPCGExValencyAssetEntry> AllEntries = GetAllAssetEntries();
	TArray<TSoftObjectPtr<UObject>> Assets;
	Assets.Reserve(AllEntries.Num());
	for (const FPCGExValencyAssetEntry& Entry : AllEntries)
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

void APCGExValencyCage::RegisterManualAsset(const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor)
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

	// Check for duplicates in manual entries
	for (const FPCGExValencyAssetEntry& Existing : ManualAssetEntries)
	{
		if (Existing.Asset == Asset)
		{
			if (!bPreserveLocalTransforms)
			{
				return;
			}
			if (Existing.LocalTransform.Equals(NewEntry.LocalTransform, 0.1f))
			{
				return;
			}
		}
	}

	ManualAssetEntries.Add(NewEntry);
	OnAssetRegistrationChanged();
}

void APCGExValencyCage::UnregisterManualAsset(const TSoftObjectPtr<UObject>& Asset)
{
	const int32 RemovedCount = ManualAssetEntries.RemoveAll([&Asset](const FPCGExValencyAssetEntry& Entry)
	{
		return Entry.Asset == Asset;
	});

	if (RemovedCount > 0)
	{
		OnAssetRegistrationChanged();
	}
}

void APCGExValencyCage::ClearManualAssets()
{
	if (ManualAssetEntries.Num() > 0)
	{
		ManualAssetEntries.Empty();
		OnAssetRegistrationChanged();
	}
}

void APCGExValencyCage::ClearScannedAssets()
{
	if (ScannedAssetEntries.Num() > 0)
	{
		ScannedAssetEntries.Empty();
		DiscoveredMaterialVariants.Empty();
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

	// Clear previous scanned entries and material variants (manual entries preserved)
	ScannedAssetEntries.Empty();
	DiscoveredMaterialVariants.Empty();

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

	// Lambda to add scanned entry (with duplicate check)
	auto AddScannedEntry = [this](const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor)
	{
		if (Asset.IsNull())
		{
			return;
		}

		FPCGExValencyAssetEntry NewEntry;
		NewEntry.Asset = Asset;
		NewEntry.SourceActor = SourceActor;
		NewEntry.AssetType = DetectAssetType(Asset);

		if (bPreserveLocalTransforms && SourceActor)
		{
			const FTransform CageTransform = GetActorTransform();
			const FTransform ActorTransform = SourceActor->GetActorTransform();
			NewEntry.LocalTransform = ActorTransform.GetRelativeTransform(CageTransform);
		}

		// Check for duplicates in scanned entries
		for (const FPCGExValencyAssetEntry& Existing : ScannedAssetEntries)
		{
			if (Existing.Asset == Asset)
			{
				if (!bPreserveLocalTransforms)
				{
					return;
				}
				if (Existing.LocalTransform.Equals(NewEntry.LocalTransform, 0.1f))
				{
					return;
				}
			}
		}

		ScannedAssetEntries.Add(NewEntry);
	};

	// Register found actors and discover material variants
	for (AActor* Actor : ContainedActors)
	{
		if (UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UStaticMesh* Mesh = SMC->GetStaticMesh())
			{
				const FSoftObjectPath MeshPath(Mesh);

				// Always discover material variants, even for duplicate meshes
				TArray<FPCGExValencyMaterialOverride> Overrides;
				ExtractMaterialOverrides(SMC, Overrides);
				if (Overrides.Num() > 0)
				{
					RecordMaterialVariant(MeshPath, Overrides);
				}

				// Add to scanned entries (may skip if duplicate)
				AddScannedEntry(TSoftObjectPtr<UObject>(Mesh), Actor);
			}
		}
		else if (UClass* ActorClass = Actor->GetClass())
		{
			// Check if it's a Blueprint
			if (UBlueprint* BP = Cast<UBlueprint>(ActorClass->ClassGeneratedBy))
			{
				AddScannedEntry(TSoftObjectPtr<UObject>(BP), Actor);
			}
		}
	}

	OnAssetRegistrationChanged();
}

void APCGExValencyCage::OnAssetRegistrationChanged()
{
	// Notify containing volumes if auto-rebuild is enabled
	// TODO: Trigger rule rebuild

	// Mark as needing save
	Modify();
}

void APCGExValencyCage::ExtractMaterialOverrides(
	const UStaticMeshComponent* MeshComponent,
	TArray<FPCGExValencyMaterialOverride>& OutOverrides)
{
	OutOverrides.Empty();

	if (!MeshComponent)
	{
		return;
	}

	const UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}

	const int32 NumMaterials = MeshComponent->GetNumMaterials();
	for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
	{
		UMaterialInterface* CurrentMaterial = MeshComponent->GetMaterial(SlotIndex);
		UMaterialInterface* DefaultMaterial = StaticMesh->GetMaterial(SlotIndex);

		// Only track if material differs from mesh's default
		if (CurrentMaterial && CurrentMaterial != DefaultMaterial)
		{
			FPCGExValencyMaterialOverride& Override = OutOverrides.AddDefaulted_GetRef();
			Override.SlotIndex = SlotIndex;
			Override.Material = CurrentMaterial;
		}
	}
}

void APCGExValencyCage::RecordMaterialVariant(
	const FSoftObjectPath& MeshPath,
	const TArray<FPCGExValencyMaterialOverride>& Overrides)
{
	if (Overrides.Num() == 0)
	{
		return;
	}

	// Find or create variants array for this mesh
	TArray<FPCGExValencyMaterialVariant>& Variants = DiscoveredMaterialVariants.FindOrAdd(MeshPath);

	// Check if this exact configuration already exists
	FPCGExValencyMaterialVariant NewVariant;
	NewVariant.Overrides = Overrides;
	NewVariant.DiscoveryCount = 1;

	for (FPCGExValencyMaterialVariant& ExistingVariant : Variants)
	{
		if (ExistingVariant == NewVariant)
		{
			// Increment discovery count (becomes weight)
			ExistingVariant.DiscoveryCount++;
			return;
		}
	}

	// New unique variant
	Variants.Add(MoveTemp(NewVariant));
}
