// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyAssetPalette.h"

#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "PCGDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Volumes/ValencyContextVolume.h"
#include "PCGExValencyEditorSettings.h"
#include "Core/PCGExValencyLog.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"

APCGExValencyCage::APCGExValencyCage()
{
	// Standard cage setup
}

void APCGExValencyCage::PostEditMove(bool bFinished)
{
	// Capture current scanned assets before Super (which may trigger volume membership changes)
	TArray<FPCGExValencyAssetEntry> OldScannedAssets;
	if (bFinished && bAutoRegisterContainedAssets && AValencyContextVolume::IsValencyModeActive())
	{
		OldScannedAssets = ScannedAssetEntries;
	}

	// Let base class handle volume membership changes, connections, etc.
	Super::PostEditMove(bFinished);

	// After drag finishes, re-scan for assets if auto-registration is enabled
	if (bFinished && bAutoRegisterContainedAssets && AValencyContextVolume::IsValencyModeActive())
	{
		// Re-scan contained assets
		ScanAndRegisterContainedAssets();

		// Check if scanned assets changed
		if (HaveScannedAssetsChanged(OldScannedAssets))
		{
			TriggerAutoRebuildIfNeeded();
		}
	}
}

void APCGExValencyCage::BeginDestroy()
{
	// Clean up ghost mesh components before destruction
	ClearMirrorGhostMeshes();

	Super::BeginDestroy();
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

	// If mirroring other sources
	if (MirrorSources.Num() > 0)
	{
		int32 ValidCount = 0;
		for (const TObjectPtr<AActor>& Source : MirrorSources)
		{
			if (Source)
			{
				ValidCount++;
			}
		}
		if (ValidCount > 0)
		{
			return FString::Printf(TEXT("Cage (Mirror: %d sources)"), ValidCount);
		}
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

	// Stamp cage's ModuleSettings onto each entry
	// This allows entries to carry their source's weight/constraints through mirroring
	for (FPCGExValencyAssetEntry& Entry : AllEntries)
	{
		Entry.Settings = ModuleSettings;
		Entry.bHasSettings = true;
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

		// Skip actors that should be ignored based on volume rules
		if (ShouldIgnoreActor(Actor))
		{
			continue;
		}

		// Use virtual containment check
		if (IsActorInside(Actor))
		{
			ContainedActors.Add(Actor);
		}
	}

	// Also check child actors (always included regardless of bounds, but still respect ignore rules)
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);

	for (AActor* Child : ChildActors)
	{
		if (Child && !Cast<APCGExValencyCageBase>(Child) && !ShouldIgnoreActor(Child))
		{
			ContainedActors.AddUnique(Child);
		}
	}

	// Lambda to add scanned entry (with duplicate check, including material variants)
	auto AddScannedEntry = [this](const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor, const FPCGExValencyMaterialVariant* InMaterialVariant)
	{
		if (Asset.IsNull())
		{
			return;
		}

		FPCGExValencyAssetEntry NewEntry;
		NewEntry.Asset = Asset;
		NewEntry.SourceActor = SourceActor;
		NewEntry.AssetType = DetectAssetType(Asset);

		// Store material variant on the entry if provided
		if (InMaterialVariant && InMaterialVariant->Overrides.Num() > 0)
		{
			NewEntry.MaterialVariant = *InMaterialVariant;
			NewEntry.bHasMaterialVariant = true;
		}

		// Compute preserved local transform based on flags
		if (SourceActor)
		{
			NewEntry.LocalTransform = ComputePreservedLocalTransform(SourceActor->GetActorTransform());
		}

		// Check for duplicates in scanned entries
		// Now considers material variants as a differentiating factor
		for (FPCGExValencyAssetEntry& Existing : ScannedAssetEntries)
		{
			if (Existing.Asset == Asset)
			{
				// If both have material variants, check if they match
				if (Existing.bHasMaterialVariant && NewEntry.bHasMaterialVariant)
				{
					if (Existing.MaterialVariant == NewEntry.MaterialVariant)
					{
						// Same asset, same material variant - check transform
						if (!bPreserveLocalTransforms || Existing.LocalTransform.Equals(NewEntry.LocalTransform, 0.1f))
						{
							// Increment discovery count as weight
							Existing.MaterialVariant.DiscoveryCount++;
							return;
						}
					}
					// Different material variants - continue to add as separate entry
				}
				else if (!Existing.bHasMaterialVariant && !NewEntry.bHasMaterialVariant)
				{
					// Both have default materials - check transform
					if (!bPreserveLocalTransforms || Existing.LocalTransform.Equals(NewEntry.LocalTransform, 0.1f))
					{
						return;
					}
				}
				// One has material variant, one doesn't - they are different entries, continue
			}
		}

		ScannedAssetEntries.Add(NewEntry);

		// Also record to legacy map for backward compatibility with existing builder code
		if (NewEntry.bHasMaterialVariant)
		{
			RecordMaterialVariant(Asset.ToSoftObjectPath(), NewEntry.MaterialVariant.Overrides);
		}
	};

	// Register found actors and discover material variants
	for (AActor* Actor : ContainedActors)
	{
		if (UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>())
		{
			if (UStaticMesh* Mesh = SMC->GetStaticMesh())
			{
				// Extract material overrides for this specific actor
				TArray<FPCGExValencyMaterialOverride> Overrides;
				ExtractMaterialOverrides(SMC, Overrides);

				// Create material variant if overrides exist
				const bool bHasOverrides = Overrides.Num() > 0;
				FPCGExValencyMaterialVariant Variant;
				if (bHasOverrides)
				{
					Variant.Overrides = MoveTemp(Overrides);
					Variant.DiscoveryCount = 1;
				}

				// Add to scanned entries with material variant info
				AddScannedEntry(TSoftObjectPtr<UObject>(Mesh), Actor, bHasOverrides ? &Variant : nullptr);
			}
		}
		else if (UClass* ActorClass = Actor->GetClass())
		{
			// Check if it's a Blueprint
			if (UBlueprint* BP = Cast<UBlueprint>(ActorClass->ClassGeneratedBy))
			{
				AddScannedEntry(TSoftObjectPtr<UObject>(BP), Actor, nullptr);
			}
		}
	}

	OnAssetRegistrationChanged();
}

void APCGExValencyCage::OnAssetRegistrationChanged()
{
	// Mark as needing save
	Modify();
}

bool APCGExValencyCage::HaveScannedAssetsChanged(const TArray<FPCGExValencyAssetEntry>& OldScannedAssets) const
{
	// Quick count check
	if (OldScannedAssets.Num() != ScannedAssetEntries.Num())
	{
		return true;
	}

	// Compare individual entries
	for (const FPCGExValencyAssetEntry& NewEntry : ScannedAssetEntries)
	{
		bool bFound = false;
		for (const FPCGExValencyAssetEntry& OldEntry : OldScannedAssets)
		{
			if (OldEntry.Asset == NewEntry.Asset)
			{
				// If preserving transforms, also check transform equality
				if (bPreserveLocalTransforms)
				{
					if (OldEntry.LocalTransform.Equals(NewEntry.LocalTransform, 0.1f))
					{
						bFound = true;
						break;
					}
				}
				else
				{
					bFound = true;
					break;
				}
			}
		}
		if (!bFound)
		{
			return true;
		}
	}

	return false;
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

FTransform APCGExValencyCage::ComputePreservedLocalTransform(const FTransform& AssetWorldTransform) const
{
	if (!bPreserveLocalTransforms)
	{
		return FTransform::Identity;
	}

	const FTransform CageTransform = GetActorTransform();
	const FTransform LocalTransform = AssetWorldTransform.GetRelativeTransform(CageTransform);

	// Build result transform based on which flags are set
	FTransform Result = FTransform::Identity;

	if (ShouldPreserveTranslation())
	{
		Result.SetTranslation(LocalTransform.GetTranslation());
	}

	if (ShouldPreserveRotation())
	{
		Result.SetRotation(LocalTransform.GetRotation());
	}

	if (ShouldPreserveScale())
	{
		Result.SetScale3D(LocalTransform.GetScale3D());
	}

	return Result;
}

#if WITH_EDITOR
void APCGExValencyCage::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Handle MirrorSources changes
	if (MemberName == GET_MEMBER_NAME_CHECKED(APCGExValencyCage, MirrorSources))
	{
		PCGEX_VALENCY_INFO(Mirror, "Cage '%s': MirrorSources changed, validating %d entries", *GetCageDisplayName(), MirrorSources.Num());

		// Validate and filter MirrorSources - only allow cages and palettes
		int32 RemovedCount = 0;
		for (int32 i = MirrorSources.Num() - 1; i >= 0; --i)
		{
			AActor* Source = MirrorSources[i];
			if (Source)
			{
				// Check if it's a valid type (cage or palette, but not self)
				const bool bIsCage = Source->IsA<APCGExValencyCage>();
				const bool bIsPalette = Source->IsA<APCGExValencyAssetPalette>();
				const bool bIsSelf = (Source == this);

				if (bIsSelf || (!bIsCage && !bIsPalette))
				{
					MirrorSources.RemoveAt(i);
					RemovedCount++;

					if (bIsSelf)
					{
						PCGEX_VALENCY_WARNING(Mirror, "  Cage '%s': Cannot mirror self - removed", *GetCageDisplayName());
					}
					else
					{
						PCGEX_VALENCY_WARNING(Mirror, "  Cage '%s': Invalid mirror source '%s' (type: %s) - must be Cage or AssetPalette",
							*GetCageDisplayName(), *Source->GetName(), *Source->GetClass()->GetName());
					}
				}
				else
				{
					PCGEX_VALENCY_VERBOSE(Mirror, "  Valid mirror source: '%s' (%s)",
						*Source->GetName(), bIsCage ? TEXT("Cage") : TEXT("Palette"));
				}
			}
		}

		if (RemovedCount > 0)
		{
			PCGEX_VALENCY_INFO(Mirror, "  Removed %d invalid entries, %d valid sources remain", RemovedCount, MirrorSources.Num());
		}

		// Notify tracker that our references changed (rebuilds dependency graph)
		if (FValencyReferenceTracker* Tracker = FPCGExValencyCageEditorMode::GetActiveReferenceTracker())
		{
			Tracker->OnActorReferencesChanged(this);
		}

		// Refresh ghost meshes
		RefreshMirrorGhostMeshes();

		// Trigger rebuild for containing volumes (with debouncing for interactive changes)
		if (UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			TriggerAutoRebuildIfNeeded();
			// Also cascade to cages that mirror THIS cage (their effective content has changed)
			TriggerAutoRebuildForMirroringCages();
		}

		// Redraw viewports
		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
	}
	// Handle other mirror-related property changes
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCage, bShowMirrorGhostMeshes))
	{
		PCGEX_VALENCY_VERBOSE(Mirror, "Cage '%s': bShowMirrorGhostMeshes changed to %s",
			*GetCageDisplayName(), bShowMirrorGhostMeshes ? TEXT("true") : TEXT("false"));
		RefreshMirrorGhostMeshes();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCage, bRecursiveMirror))
	{
		RefreshMirrorGhostMeshes();
		if (UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			TriggerAutoRebuildIfNeeded();
		}
	}
	else
	{
		// Check if any property in the chain has PCGEX_ValencyRebuild metadata
		bool bShouldRebuild = false;

		if (const FProperty* Property = PropertyChangedEvent.Property)
		{
			if (Property->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
			{
				bShouldRebuild = true;
			}
		}

		if (!bShouldRebuild && PropertyChangedEvent.MemberProperty)
		{
			if (PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
			{
				bShouldRebuild = true;
			}
		}

		// Debounce interactive changes (dragging sliders) to prevent spam
		if (bShouldRebuild && !UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			bShouldRebuild = false;
		}

		if (bShouldRebuild)
		{
			PCGEX_VALENCY_INFO(Building, "Cage '%s': Property '%s' with ValencyRebuild metadata changed - triggering rebuild",
				*GetCageDisplayName(), *PropertyName.ToString());
			TriggerAutoRebuildIfNeeded();
		}
	}
}
#endif

void APCGExValencyCage::RefreshMirrorGhostMeshes()
{
	// Clear existing ghost meshes first
	ClearMirrorGhostMeshes();

	// Get settings
	const UPCGExValencyEditorSettings* Settings = UPCGExValencyEditorSettings::Get();

	// Early out if ghosting is disabled (either globally or per-cage)
	if (!Settings || !Settings->bEnableGhostMeshes || !bShowMirrorGhostMeshes || MirrorSources.Num() == 0)
	{
		return;
	}

	// Get the shared ghost material from settings
	UMaterialInterface* GhostMaterial = Settings->GetGhostMaterial();

	// Collect entries from all mirror sources
	TArray<FPCGExValencyAssetEntry> AllEntries;
	TSet<AActor*> VisitedSources; // Prevent infinite recursion

	// Lambda to collect entries from a source (with optional recursion)
	TFunction<void(AActor*)> CollectFromSource = [&](AActor* Source)
	{
		if (!Source || Source == this || VisitedSources.Contains(Source))
		{
			return;
		}
		VisitedSources.Add(Source);

		// Check if it's a cage
		if (APCGExValencyCage* SourceCage = Cast<APCGExValencyCage>(Source))
		{
			// Get entries from the cage
			AllEntries.Append(SourceCage->GetAllAssetEntries());

			// Recursively collect from cage's mirror sources
			if (bRecursiveMirror)
			{
				for (const TObjectPtr<AActor>& NestedSource : SourceCage->MirrorSources)
				{
					CollectFromSource(NestedSource);
				}
			}
		}
		// Check if it's an asset palette
		else if (APCGExValencyAssetPalette* SourcePalette = Cast<APCGExValencyAssetPalette>(Source))
		{
			AllEntries.Append(SourcePalette->GetAllAssetEntries());
		}
	};

	// Collect from all mirror sources
	for (const TObjectPtr<AActor>& Source : MirrorSources)
	{
		CollectFromSource(Source);
	}

	if (AllEntries.Num() == 0)
	{
		return;
	}

	// Get this cage's rotation for applying to local transforms
	const FQuat CageRotation = GetActorQuat();
	const FVector CageLocation = GetActorLocation();

	// Create ghost mesh components for each mesh asset
	for (const FPCGExValencyAssetEntry& Entry : AllEntries)
	{
		if (Entry.AssetType != EPCGExValencyAssetType::Mesh)
		{
			continue;
		}

		// Try to get the mesh (prefer already-loaded, fallback to sync load)
		UStaticMesh* Mesh = Cast<UStaticMesh>(Entry.Asset.Get());
		if (!Mesh)
		{
			// Asset not loaded - try sync load (editor-only, game thread safe)
			Mesh = Cast<UStaticMesh>(Entry.Asset.LoadSynchronous());
		}
		if (!Mesh)
		{
			continue;
		}

		// Create ghost mesh component
		UStaticMeshComponent* GhostComp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		GhostComp->SetStaticMesh(Mesh);
		GhostComp->SetMobility(EComponentMobility::Movable);
		GhostComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GhostComp->bSelectable = false;
		GhostComp->SetCastShadow(false);

		// Apply the shared ghost material to all slots
		if (GhostMaterial)
		{
			const int32 NumMaterials = Mesh->GetStaticMaterials().Num();
			for (int32 i = 0; i < NumMaterials; ++i)
			{
				GhostComp->SetMaterial(i, GhostMaterial);
			}
		}

		// Compute transform: cage location + rotated local transform from source
		FTransform GhostTransform;
		if (!Entry.LocalTransform.Equals(FTransform::Identity, 0.1f))
		{
			// Rotate the source's local offset by this cage's rotation
			const FVector RotatedOffset = CageRotation.RotateVector(Entry.LocalTransform.GetTranslation());
			const FQuat CombinedRotation = CageRotation * Entry.LocalTransform.GetRotation();

			GhostTransform.SetLocation(CageLocation + RotatedOffset);
			GhostTransform.SetRotation(CombinedRotation);
			GhostTransform.SetScale3D(Entry.LocalTransform.GetScale3D());
		}
		else
		{
			GhostTransform.SetLocation(CageLocation);
			GhostTransform.SetRotation(CageRotation);
			GhostTransform.SetScale3D(FVector::OneVector);
		}

		GhostComp->SetWorldTransform(GhostTransform);

		// Attach and register
		GhostComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		GhostComp->RegisterComponent();

		GhostMeshComponents.Add(GhostComp);
	}
}

void APCGExValencyCage::ClearMirrorGhostMeshes()
{
	for (TObjectPtr<UStaticMeshComponent>& GhostComp : GhostMeshComponents)
	{
		if (GhostComp)
		{
			GhostComp->DestroyComponent();
		}
	}
	GhostMeshComponents.Empty();
}

void APCGExValencyCage::FindMirroringCages(TArray<APCGExValencyCage*>& OutCages) const
{
	OutCages.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find all cages that have this cage in their MirrorSources
	for (TActorIterator<APCGExValencyCage> It(World); It; ++It)
	{
		APCGExValencyCage* Cage = *It;
		if (!Cage || Cage == this)
		{
			continue;
		}

		for (const TObjectPtr<AActor>& Source : Cage->MirrorSources)
		{
			if (Source == this)
			{
				OutCages.Add(Cage);
				break;
			}
		}
	}
}

bool APCGExValencyCage::TriggerAutoRebuildForMirroringCages()
{
	// Use centralized reference tracker for recursive propagation
	if (FValencyReferenceTracker* Tracker = FPCGExValencyCageEditorMode::GetActiveReferenceTracker())
	{
		return Tracker->PropagateContentChange(this);
	}
	return false;
}
