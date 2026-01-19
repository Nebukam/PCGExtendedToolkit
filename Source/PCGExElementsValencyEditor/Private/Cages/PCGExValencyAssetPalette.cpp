// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyAssetPalette.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Volumes/ValencyContextVolume.h"

#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "PCGDataAsset.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

#if WITH_EDITOR
#include "Editor.h"
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
}

void APCGExValencyAssetPalette::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// Restore transient state after level load
	UpdateShapeComponent();

	// Re-scan for contained assets if auto-registration is enabled
	if (bAutoRegisterContainedAssets && GetWorld())
	{
		ScanAndRegisterContainedAssets();
	}
#endif
}

void APCGExValencyAssetPalette::PostActorCreated()
{
	Super::PostActorCreated();

	// Auto-organize into Valency/Palettes folder
	SetFolderPath(PCGExValencyFolders::PalettesFolder);
}

void APCGExValencyAssetPalette::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Ensure shape component exists (may be null after level load since it's transient)
	UpdateShapeComponent();

	// Initial scan if auto-registration is enabled
	if (bAutoRegisterContainedAssets)
	{
		ScanAndRegisterContainedAssets();
	}
}

void APCGExValencyAssetPalette::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Update shape when detection settings change
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, DetectionShape) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, DetectionExtent))
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

	// Trigger rebuild when manual assets change
	if (MemberName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, ManualAssetEntries))
	{
		OnAssetRegistrationChanged();
	}

	// Trigger rebuild when module settings change
	if (MemberName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, ModuleSettings))
	{
		TriggerAutoRebuildForMirroringCages();
	}

	// Trigger rebuild when transform preservation settings change
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, bPreserveLocalTransforms) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyAssetPalette, LocalTransformFlags))
	{
		if (bAutoRegisterContainedAssets)
		{
			ScanAndRegisterContainedAssets();
		}
		TriggerAutoRebuildForMirroringCages();
	}

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

	if (bShouldRebuild)
	{
		TriggerAutoRebuildForMirroringCages();
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
			TriggerAutoRebuildForMirroringCages();
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

TArray<FPCGExValencyAssetEntry> APCGExValencyAssetPalette::GetAllAssetEntries() const
{
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

	switch (DetectionShape)
	{
	case EPCGExAssetPaletteShape::Box:
		return FMath::Abs(LocalLocation.X) <= DetectionExtent.X &&
			   FMath::Abs(LocalLocation.Y) <= DetectionExtent.Y &&
			   FMath::Abs(LocalLocation.Z) <= DetectionExtent.Z;

	case EPCGExAssetPaletteShape::Sphere:
		return LocalLocation.Size() <= DetectionExtent.X;

	case EPCGExAssetPaletteShape::Capsule:
		{
			const float HalfHeight = DetectionExtent.Z;
			const float Radius = DetectionExtent.X;
			const float ClampedZ = FMath::Clamp(LocalLocation.Z, -HalfHeight, HalfHeight);
			const FVector ClosestPoint(0, 0, ClampedZ);
			return FVector::Dist(LocalLocation, ClosestPoint) <= Radius;
		}

	default:
		return false;
	}
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

	// Helper to detect asset type
	auto DetectAssetType = [](const TSoftObjectPtr<UObject>& Asset) -> EPCGExValencyAssetType
	{
		if (Asset.IsNull())
		{
			return EPCGExValencyAssetType::Unknown;
		}

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

		return EPCGExValencyAssetType::Unknown;
	};

	// Lambda to add scanned entry (with duplicate check, including material variants)
	auto AddScannedEntry = [this, &DetectAssetType](const TSoftObjectPtr<UObject>& Asset, AActor* SourceActor, const FPCGExValencyMaterialVariant* InMaterialVariant)
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

		if (SourceActor)
		{
			NewEntry.LocalTransform = ComputePreservedLocalTransform(SourceActor->GetActorTransform());
		}

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

	OnAssetRegistrationChanged();
}

FTransform APCGExValencyAssetPalette::ComputePreservedLocalTransform(const FTransform& AssetWorldTransform) const
{
	if (!bPreserveLocalTransforms)
	{
		return FTransform::Identity;
	}

	const FTransform PaletteTransform = GetActorTransform();
	const FTransform LocalTransform = AssetWorldTransform.GetRelativeTransform(PaletteTransform);

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

bool APCGExValencyAssetPalette::ShouldPreserveTranslation() const
{
	return bPreserveLocalTransforms &&
		   EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Translation);
}

bool APCGExValencyAssetPalette::ShouldPreserveRotation() const
{
	return bPreserveLocalTransforms &&
		   EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Rotation);
}

bool APCGExValencyAssetPalette::ShouldPreserveScale() const
{
	return bPreserveLocalTransforms &&
		   EnumHasAnyFlags(static_cast<EPCGExLocalTransformFlags>(LocalTransformFlags), EPCGExLocalTransformFlags::Scale);
}

void APCGExValencyAssetPalette::UpdateShapeComponent()
{
	// Destroy existing shape component
	if (ShapeComponent)
	{
		ShapeComponent->DestroyComponent();
		ShapeComponent = nullptr;
	}

	// Create appropriate shape component
	switch (DetectionShape)
	{
	case EPCGExAssetPaletteShape::Box:
		{
			UBoxComponent* Box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
			Box->SetBoxExtent(DetectionExtent);
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->SetLineThickness(2.0f);
			Box->ShapeColor = PaletteColor.ToFColor(true);
			ShapeComponent = Box;
		}
		break;

	case EPCGExAssetPaletteShape::Sphere:
		{
			USphereComponent* Sphere = NewObject<USphereComponent>(this, NAME_None, RF_Transient);
			Sphere->SetSphereRadius(DetectionExtent.X);
			Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sphere->SetLineThickness(2.0f);
			Sphere->ShapeColor = PaletteColor.ToFColor(true);
			ShapeComponent = Sphere;
		}
		break;

	case EPCGExAssetPaletteShape::Capsule:
		{
			UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(this, NAME_None, RF_Transient);
			Capsule->SetCapsuleSize(DetectionExtent.X, DetectionExtent.Z);
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Capsule->SetLineThickness(2.0f);
			Capsule->ShapeColor = PaletteColor.ToFColor(true);
			ShapeComponent = Capsule;
		}
		break;
	}

	if (ShapeComponent)
	{
		ShapeComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		ShapeComponent->RegisterComponent();
	}
}

void APCGExValencyAssetPalette::OnAssetRegistrationChanged()
{
	Modify();

	// Trigger rebuild for cages that mirror this palette
	TriggerAutoRebuildForMirroringCages();

#if WITH_EDITOR
	// Redraw viewports to show updated state
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}
#endif
}

void APCGExValencyAssetPalette::ExtractMaterialOverrides(
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

		if (CurrentMaterial && CurrentMaterial != DefaultMaterial)
		{
			FPCGExValencyMaterialOverride& Override = OutOverrides.AddDefaulted_GetRef();
			Override.SlotIndex = SlotIndex;
			Override.Material = CurrentMaterial;
		}
	}
}

void APCGExValencyAssetPalette::RecordMaterialVariant(
	const FSoftObjectPath& MeshPath,
	const TArray<FPCGExValencyMaterialOverride>& Overrides)
{
	if (Overrides.Num() == 0)
	{
		return;
	}

	TArray<FPCGExValencyMaterialVariant>& Variants = DiscoveredMaterialVariants.FindOrAdd(MeshPath);

	FPCGExValencyMaterialVariant NewVariant;
	NewVariant.Overrides = Overrides;
	NewVariant.DiscoveryCount = 1;

	for (FPCGExValencyMaterialVariant& ExistingVariant : Variants)
	{
		if (ExistingVariant == NewVariant)
		{
			ExistingVariant.DiscoveryCount++;
			return;
		}
	}

	Variants.Add(MoveTemp(NewVariant));
}

void APCGExValencyAssetPalette::FindMirroringCages(TArray<APCGExValencyCage*>& OutCages) const
{
	OutCages.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find all cages that have this palette in their MirrorSources
	for (TActorIterator<APCGExValencyCage> It(World); It; ++It)
	{
		APCGExValencyCage* Cage = *It;
		if (!Cage)
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

bool APCGExValencyAssetPalette::TriggerAutoRebuildForMirroringCages()
{
	// Only process when Valency mode is active
	if (!AValencyContextVolume::IsValencyModeActive())
	{
		return false;
	}

	// Find cages that mirror this palette
	TArray<APCGExValencyCage*> MirroringCages;
	FindMirroringCages(MirroringCages);

	if (MirroringCages.Num() == 0)
	{
		return false;
	}

	// Trigger rebuild through one of the mirroring cages
	// The multi-volume aggregation will handle all related volumes
	for (APCGExValencyCage* Cage : MirroringCages)
	{
		if (Cage)
		{
			// Use the cage's existing rebuild mechanism
			// This respects the volume's bAutoRebuildOnChange setting
			if (Cage->TriggerAutoRebuildIfNeeded())
			{
				return true;
			}
		}
	}

	return false;
}

bool APCGExValencyAssetPalette::HaveScannedAssetsChanged(const TArray<FPCGExValencyAssetEntry>& OldScannedAssets) const
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
