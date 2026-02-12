// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Cages/PCGExValencyAssetUtils.h"

#include "PCGExValencyEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "PCGDataAsset.h"
#include "Components/StaticMeshComponent.h"
#include "Volumes/ValencyContextVolume.h"
#include "PCGExValencyEditorSettings.h"
#include "PCGExValencyMacros.h"
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
			RequestRebuild(EValencyRebuildReason::AssetChange);
		}
	}
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

// Asset type detection now in PCGExValencyAssetUtils::DetectAssetType

void APCGExValencyCage::RegisterManualAsset(const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor)
{
	if (Asset.IsNull())
	{
		return;
	}

	FPCGExValencyAssetEntry NewEntry;
	NewEntry.Asset = Asset;
	NewEntry.SourceActor = SourceActor;
	NewEntry.AssetType = PCGExValencyAssetUtils::DetectAssetType(Asset);

	// Compute local transform if we have a source actor and preservation is enabled
	if (bPreserveLocalTransforms && SourceActor)
	{
		const FTransform CageTransform = GetActorTransform();
		const FTransform ActorTransform = SourceActor->GetActorTransform();
		NewEntry.LocalTransform = ActorTransform.GetRelativeTransform(CageTransform);
		NewEntry.bPreserveLocalTransform = true;
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
		NewEntry.AssetType = PCGExValencyAssetUtils::DetectAssetType(Asset);

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

		// Mark entry to preserve its local transform if the cage has that setting enabled
		NewEntry.bPreserveLocalTransform = bPreserveLocalTransforms;

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

	// Request rebuild for this cage (assets changed)
	RequestRebuild(EValencyRebuildReason::AssetChange);

	// Propagate to dependent cages/patterns (refreshes ghosts and triggers rebuild cascade)
	if (FValencyReferenceTracker* Tracker = FPCGExValencyCageEditorMode::GetActiveReferenceTracker())
	{
		Tracker->PropagateContentChange(this, /*bRefreshGhosts=*/true, /*bTriggerRebuild=*/true);
	}

	PCGEX_VALENCY_REDRAW_ALL_VIEWPORT
}

bool APCGExValencyCage::HaveScannedAssetsChanged(const TArray<FPCGExValencyAssetEntry>& OldScannedAssets) const
{
	return PCGExValencyAssetUtils::HaveScannedAssetsChanged(OldScannedAssets, ScannedAssetEntries, bPreserveLocalTransforms);
}

void APCGExValencyCage::ExtractMaterialOverrides(
	const UStaticMeshComponent* MeshComponent,
	TArray<FPCGExValencyMaterialOverride>& OutOverrides)
{
	PCGExValencyAssetUtils::ExtractMaterialOverrides(MeshComponent, OutOverrides);
}

void APCGExValencyCage::RecordMaterialVariant(
	const FSoftObjectPath& MeshPath,
	const TArray<FPCGExValencyMaterialOverride>& Overrides)
{
	PCGExValencyAssetUtils::RecordMaterialVariant(MeshPath, Overrides, DiscoveredMaterialVariants);
}

FTransform APCGExValencyCage::ComputePreservedLocalTransform(const FTransform& AssetWorldTransform) const
{
	return PCGExValencyAssetUtils::ComputePreservedLocalTransform(
		AssetWorldTransform, GetActorTransform(), bPreserveLocalTransforms, LocalTransformFlags);
}

void APCGExValencyCage::OnPostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::OnPostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Properties with PCGEX_ValencyGhostRefresh that need asset registration propagation
	// (base class already handled ClearGhostMeshes + RefreshGhostMeshes)
	{
		bool bGhostRefresh = false;
		if (const FProperty* Property = PropertyChangedEvent.Property)
		{
			bGhostRefresh = Property->HasMetaData(TEXT("PCGEX_ValencyGhostRefresh"));
		}
		if (!bGhostRefresh && PropertyChangedEvent.MemberProperty)
		{
			bGhostRefresh = PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyGhostRefresh"));
		}

		if (bGhostRefresh)
		{
			OnAssetRegistrationChanged();
		}
	}

	// Handle MirrorSources changes (validation and tracker notification)
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

		// Notify tracker that our MirrorSources changed (incrementally updates dependency graph)
		if (FValencyReferenceTracker* Tracker = FPCGExValencyCageEditorMode::GetActiveReferenceTracker())
		{
			Tracker->OnMirrorSourcesChanged(this);
		}

		PCGEX_VALENCY_REDRAW_ALL_VIEWPORT
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCage, bShowMirrorGhostMeshes))
	{
		PCGEX_VALENCY_VERBOSE(Mirror, "Cage '%s': bShowMirrorGhostMeshes changed to %s",
			*GetCageDisplayName(), bShowMirrorGhostMeshes ? TEXT("true") : TEXT("false"));
		ClearGhostMeshes();
		RefreshGhostMeshes();
	}
}

void APCGExValencyCage::RefreshGhostMeshes()
{
	// Clear existing ghost meshes first
	ClearGhostMeshes();

	// Get settings
	const UPCGExValencyEditorSettings* Settings = UPCGExValencyEditorSettings::Get();

	// Early out if ghosting is disabled (either globally or per-cage)
	if (!Settings || !Settings->bEnableGhostMeshes || !bShowMirrorGhostMeshes || MirrorSources.Num() == 0 || Settings->MaxCageGhostMeshes == 0)
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

	// Get the limit (-1 = unlimited)
	const int32 MaxGhosts = Settings->MaxCageGhostMeshes;
	int32 GhostCount = 0;

	// Create ghost mesh components for each mesh asset
	for (const FPCGExValencyAssetEntry& Entry : AllEntries)
	{
		// Check if we've hit the limit
		if (MaxGhosts >= 0 && GhostCount >= MaxGhosts)
		{
			break;
		}

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
		GhostComp->ComponentTags.Add(PCGExValencyTags::GhostMeshTag);
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

			GhostCount++;
	}
}

void APCGExValencyCage::FindMirroringCages(TArray<APCGExValencyCage*>& OutCages) const
{
	PCGExValencyAssetUtils::FindMirroringCages(this, GetWorld(), OutCages);
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
