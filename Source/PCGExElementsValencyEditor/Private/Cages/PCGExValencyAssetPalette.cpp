// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyAssetPalette.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyAssetUtils.h"
#include "Volumes/ValencyContextVolume.h"

#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "PCGDataAsset.h"
#include "PCGExValencyMacros.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_EDITOR
#include "Editor.h"
#include "PCGExValencyEditorSettings.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"
#endif

namespace PCGExValencyFolders
{
	const FName PalettesFolder = FName(TEXT("Valency/Palettes"));
}

APCGExValencyAssetPalette::APCGExValencyAssetPalette()
{
	PrimaryActorTick.bCanEverTick = false;

	// Configure as editor-only
	bNetLoadOnClient = false;
	bReplicates = false;

	// Create root component
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Create box component as default subobject (persists with the actor)
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxBounds"));
	BoxComponent->SetupAttachment(Root);
	BoxComponent->SetBoxExtent(DetectionExtent);
	BoxComponent->SetLineThickness(2.0f);
	BoxComponent->ShapeColor = PaletteColor.ToFColor(true);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoxComponent->SetHiddenInGame(true);
	BoxComponent->bVisibleInReflectionCaptures = false;
}

void APCGExValencyAssetPalette::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// Update shape visibility and size to match current settings
	UpdateShapeComponent();
#endif
}

void APCGExValencyAssetPalette::PostActorCreated()
{
	Super::PostActorCreated();

	// Auto-organize into Valency/Palettes folder
	SetFolderPath(PCGExValencyFolders::PalettesFolder);

	// Ensure shape visibility matches current settings
	UpdateShapeComponent();

	// Newly created palettes don't need deferred initialization
	// (they start empty and user adds content interactively)
	bNeedsInitialScan = false;
}

void APCGExValencyAssetPalette::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Update shape visibility to match current settings
	UpdateShapeComponent();

	// Note: We don't scan here anymore - palettes use lazy initialization via EnsureInitialized()
	// This is called automatically when a cage accesses the palette's content
}

void APCGExValencyAssetPalette::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Update box when detection settings or color change
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, DetectionExtent) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, PaletteColor))
	{
		UpdateShapeComponent();

		// Re-scan if extent changed and auto-registration is enabled
		if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, DetectionExtent) && bAutoRegisterContainedAssets)
		{
			ScanAndRegisterContainedAssets();
		}
	}

	// Re-scan when auto-registration is toggled on
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, bAutoRegisterContainedAssets))
	{
		if (bAutoRegisterContainedAssets)
		{
			ScanAndRegisterContainedAssets();
		}
	}

	// Trigger rebuild when manual assets change (with debouncing)
	if (MemberName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, ManualAssetEntries))
	{
		if (UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			OnAssetRegistrationChanged();
		}
	}

	// Trigger rebuild when module settings change (with debouncing)
	if (MemberName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, ModuleSettings))
	{
		if (UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
		{
			RequestRebuildForMirroringCages();
		}
	}

	// Re-scan when transform preservation settings change
	// Rebuild is handled by PCGEX_ValencyRebuild meta tag on these properties (see generic check below)
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, bPreserveLocalTransforms) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, LocalTransformFlags))
	{
		if (bAutoRegisterContainedAssets)
		{
			ScanAndRegisterContainedAssets();
		}
	}

	// Generic PCGEX_ValencyRebuild meta tag check
	// Covers bAutoRegisterContainedAssets, bPreserveLocalTransforms, LocalTransformFlags, etc.
	{
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
			RequestRebuildForMirroringCages();
		}
	}
}

void APCGExValencyAssetPalette::BeginDestroy()
{
	// Trigger rebuild for mirroring cages before destruction
	// (only if world is valid and not being torn down)
#if WITH_EDITOR
	if (UWorld* World = GetWorld())
	{
		if (!World->bIsTearingDown && !World->IsPlayInEditor())
		{
			TriggerAutoRebuildForMirroringCages();
		}
	}
#endif

	Super::BeginDestroy();
}

void APCGExValencyAssetPalette::PostEditMove(bool bFinished)
{
	// Capture current scanned assets before movement processing
	TArray<FPCGExValencyAssetEntry> OldScannedAssets;
	if (bFinished && bAutoRegisterContainedAssets && AValencyContextVolume::IsValencyModeActive())
	{
		OldScannedAssets = ScannedAssetEntries;
	}

	Super::PostEditMove(bFinished);

	// Re-scan after movement if auto-registration is enabled
	if (bFinished && bAutoRegisterContainedAssets)
	{
		ScanAndRegisterContainedAssets();

		// Check if assets changed and trigger rebuild for mirroring cages
		if (AValencyContextVolume::IsValencyModeActive() && HaveScannedAssetsChanged(OldScannedAssets))
		{
			RequestRebuildForMirroringCages();
		}
	}
}

FString APCGExValencyAssetPalette::GetPaletteDisplayName() const
{
	if (!PaletteName.IsEmpty())
	{
		return PaletteName;
	}

	const int32 ManualCount = ManualAssetEntries.Num();
	const int32 ScannedCount = ScannedAssetEntries.Num();
	const int32 TotalCount = ManualCount + ScannedCount;

	if (TotalCount > 0)
	{
		if (ManualCount > 0 && ScannedCount > 0)
		{
			return FString::Printf(TEXT("Palette [%d+%d assets]"), ManualCount, ScannedCount);
		}
		return FString::Printf(TEXT("Palette [%d assets]"), TotalCount);
	}

	return TEXT("Palette (Empty)");
}

void APCGExValencyAssetPalette::EnsureInitialized()
{
	if (bNeedsInitialScan && bAutoRegisterContainedAssets)
	{
		ScanAndRegisterContainedAssets();
		// Note: ScanAndRegisterContainedAssets clears bNeedsInitialScan
	}
}

TArray<FPCGExValencyAssetEntry> APCGExValencyAssetPalette::GetAllAssetEntries() const
{
	// Ensure we're initialized before returning entries
	// Cast away const for lazy initialization pattern
	const_cast<APCGExValencyAssetPalette*>(this)->EnsureInitialized();

	TArray<FPCGExValencyAssetEntry> AllEntries;
	AllEntries.Reserve(ManualAssetEntries.Num() + ScannedAssetEntries.Num());

	// Manual entries first
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

	// Stamp palette's ModuleSettings onto each entry
	// This allows entries to carry their source's weight/constraints through mirroring
	for (FPCGExValencyAssetEntry& Entry : AllEntries)
	{
		Entry.Settings = ModuleSettings;
		Entry.bHasSettings = true;
	}

	return AllEntries;
}

bool APCGExValencyAssetPalette::IsActorInside_Implementation(AActor* Actor) const
{
	if (!Actor || Actor == this)
	{
		return false;
	}

	return ContainsPoint(Actor->GetActorLocation());
}

bool APCGExValencyAssetPalette::ContainsPoint_Implementation(const FVector& WorldLocation) const
{
	const FVector LocalLocation = GetActorTransform().InverseTransformPosition(WorldLocation);

	return FMath::Abs(LocalLocation.X) <= DetectionExtent.X &&
		   FMath::Abs(LocalLocation.Y) <= DetectionExtent.Y &&
		   FMath::Abs(LocalLocation.Z) <= DetectionExtent.Z;
}

void APCGExValencyAssetPalette::ScanAndRegisterContainedAssets()
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

	// Clear previous scanned entries
	ScannedAssetEntries.Empty();
	DiscoveredMaterialVariants.Empty();

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

		if (SourceActor)
		{
			NewEntry.LocalTransform = ComputePreservedLocalTransform(SourceActor->GetActorTransform());
		}

		// Mark entry to preserve its local transform if the palette has that setting enabled
		NewEntry.bPreserveLocalTransform = bPreserveLocalTransforms;

		// Check for duplicates - material variants differentiate entries
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

	// Scan for actors
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor == this)
		{
			continue;
		}

		// Skip other palettes, cages, and volumes
		if (Actor->IsA<APCGExValencyAssetPalette>() || Actor->IsA<APCGExValencyCageBase>() || Actor->IsA<AValencyContextVolume>())
		{
			continue;
		}

		if (IsActorInside(Actor))
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
				if (UBlueprint* BP = Cast<UBlueprint>(ActorClass->ClassGeneratedBy))
				{
					AddScannedEntry(TSoftObjectPtr<UObject>(BP), Actor, nullptr);
				}
			}
		}
	}

	// Also check attached children
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);

	for (AActor* Child : ChildActors)
	{
		if (Child && !Child->IsA<APCGExValencyAssetPalette>() && !Child->IsA<APCGExValencyCageBase>())
		{
			if (UStaticMeshComponent* SMC = Child->FindComponentByClass<UStaticMeshComponent>())
			{
				if (UStaticMesh* Mesh = SMC->GetStaticMesh())
				{
					// Extract material overrides for this child actor
					TArray<FPCGExValencyMaterialOverride> Overrides;
					ExtractMaterialOverrides(SMC, Overrides);

					const bool bHasOverrides = Overrides.Num() > 0;
					FPCGExValencyMaterialVariant Variant;
					if (bHasOverrides)
					{
						Variant.Overrides = MoveTemp(Overrides);
						Variant.DiscoveryCount = 1;
					}

					AddScannedEntry(TSoftObjectPtr<UObject>(Mesh), Child, bHasOverrides ? &Variant : nullptr);
				}
			}
		}
	}

	// Mark as initialized - no longer needs initial scan
	bNeedsInitialScan = false;

	OnAssetRegistrationChanged();
}

FTransform APCGExValencyAssetPalette::ComputePreservedLocalTransform(const FTransform& AssetWorldTransform) const
{
	return PCGExValencyAssetUtils::ComputePreservedLocalTransform(
		AssetWorldTransform, GetActorTransform(), bPreserveLocalTransforms, LocalTransformFlags);
}

bool APCGExValencyAssetPalette::ShouldPreserveTranslation() const
{
	return bPreserveLocalTransforms && EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Translation);
}

bool APCGExValencyAssetPalette::ShouldPreserveRotation() const
{
	return bPreserveLocalTransforms && EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Rotation);
}

bool APCGExValencyAssetPalette::ShouldPreserveScale() const
{
	return bPreserveLocalTransforms && EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Scale);
}

void APCGExValencyAssetPalette::UpdateShapeComponent()
{
	if (BoxComponent)
	{
		BoxComponent->SetBoxExtent(DetectionExtent);
		BoxComponent->ShapeColor = PaletteColor.ToFColor(true);
	}
}

void APCGExValencyAssetPalette::OnAssetRegistrationChanged()
{
	Modify();

	// Trigger rebuild for cages that mirror this palette via unified dirty state system
	RequestRebuildForMirroringCages();

	PCGEX_VALENCY_REDRAW_ALL_VIEWPORT
}

void APCGExValencyAssetPalette::ExtractMaterialOverrides(
	const UStaticMeshComponent* MeshComponent,
	TArray<FPCGExValencyMaterialOverride>& OutOverrides)
{
	PCGExValencyAssetUtils::ExtractMaterialOverrides(MeshComponent, OutOverrides);
}

void APCGExValencyAssetPalette::RecordMaterialVariant(
	const FSoftObjectPath& MeshPath,
	const TArray<FPCGExValencyMaterialOverride>& Overrides)
{
	PCGExValencyAssetUtils::RecordMaterialVariant(MeshPath, Overrides, DiscoveredMaterialVariants);
}

void APCGExValencyAssetPalette::FindMirroringCages(TArray<APCGExValencyCage*>& OutCages) const
{
	PCGExValencyAssetUtils::FindMirroringCages(this, GetWorld(), OutCages);
}

bool APCGExValencyAssetPalette::TriggerAutoRebuildForMirroringCages()
{
	// Use centralized reference tracker for recursive propagation
	if (FValencyReferenceTracker* Tracker = FPCGExValencyCageEditorMode::GetActiveReferenceTracker())
	{
		return Tracker->PropagateContentChange(this);
	}
	return false;
}

void APCGExValencyAssetPalette::RequestRebuildForMirroringCages()
{
	// Find all cages that mirror this palette and request rebuild on each
	TArray<APCGExValencyCage*> MirroringCages;
	FindMirroringCages(MirroringCages);

	for (APCGExValencyCage* Cage : MirroringCages)
	{
		if (Cage)
		{
			Cage->RequestRebuild(EValencyRebuildReason::ExternalCascade);
		}
	}
}

bool APCGExValencyAssetPalette::HaveScannedAssetsChanged(const TArray<FPCGExValencyAssetEntry>& OldScannedAssets) const
{
	return PCGExValencyAssetUtils::HaveScannedAssetsChanged(OldScannedAssets, ScannedAssetEntries, bPreserveLocalTransforms);
}
