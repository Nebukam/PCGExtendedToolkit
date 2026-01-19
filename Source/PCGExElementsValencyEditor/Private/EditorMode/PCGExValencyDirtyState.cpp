// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyDirtyState.h"

#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"

void FValencyDirtyStateManager::Initialize(
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& InCachedCages,
	const TArray<TWeakObjectPtr<AValencyContextVolume>>& InCachedVolumes,
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>& InCachedPalettes)
{
	CachedCages = &InCachedCages;
	CachedVolumes = &InCachedVolumes;
	CachedPalettes = &InCachedPalettes;
	Reset();
}

void FValencyDirtyStateManager::Reset()
{
	DirtyCages.Empty();
	DirtyPalettes.Empty();
	DirtyVolumes.Empty();
}

void FValencyDirtyStateManager::MarkCageDirty(APCGExValencyCageBase* Cage, EValencyDirtyFlags Flags)
{
	if (!Cage || Flags == EValencyDirtyFlags::None)
	{
		return;
	}

	// Merge with existing flags
	EValencyDirtyFlags& ExistingFlags = DirtyCages.FindOrAdd(Cage);
	ExistingFlags |= Flags;

	UE_LOG(LogTemp, Verbose, TEXT("Valency: Marked cage '%s' dirty with flags 0x%02X"),
		*Cage->GetCageDisplayName(), static_cast<uint8>(Flags));
}

void FValencyDirtyStateManager::MarkPaletteDirty(APCGExValencyAssetPalette* Palette, EValencyDirtyFlags Flags)
{
	if (!Palette || Flags == EValencyDirtyFlags::None)
	{
		return;
	}

	// Merge with existing flags
	EValencyDirtyFlags& ExistingFlags = DirtyPalettes.FindOrAdd(Palette);
	ExistingFlags |= Flags;

	UE_LOG(LogTemp, Verbose, TEXT("Valency: Marked palette '%s' dirty with flags 0x%02X"),
		*Palette->GetPaletteDisplayName(), static_cast<uint8>(Flags));
}

void FValencyDirtyStateManager::MarkVolumeDirty(AValencyContextVolume* Volume, EValencyDirtyFlags Flags)
{
	if (!Volume || Flags == EValencyDirtyFlags::None)
	{
		return;
	}

	// Merge with existing flags
	EValencyDirtyFlags& ExistingFlags = DirtyVolumes.FindOrAdd(Volume);
	ExistingFlags |= Flags;

	UE_LOG(LogTemp, Verbose, TEXT("Valency: Marked volume dirty with flags 0x%02X"),
		static_cast<uint8>(Flags));
}

void FValencyDirtyStateManager::MarkVolumeContentsDirty(AValencyContextVolume* Volume, EValencyDirtyFlags Flags)
{
	if (!Volume || !CachedCages)
	{
		return;
	}

	// Mark all cages in the volume as dirty
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		if (APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			if (Volume->ContainsPoint(Cage->GetActorLocation()))
			{
				MarkCageDirty(Cage, Flags);
			}
		}
	}

	// Also mark palettes if they're in the volume
	if (CachedPalettes)
	{
		for (const TWeakObjectPtr<APCGExValencyAssetPalette>& PalettePtr : *CachedPalettes)
		{
			if (APCGExValencyAssetPalette* Palette = PalettePtr.Get())
			{
				if (Volume->ContainsPoint(Palette->GetActorLocation()))
				{
					MarkPaletteDirty(Palette, Flags);
				}
			}
		}
	}
}

bool FValencyDirtyStateManager::IsCageDirty(const APCGExValencyCageBase* Cage) const
{
	return Cage && DirtyCages.Contains(const_cast<APCGExValencyCageBase*>(Cage));
}

bool FValencyDirtyStateManager::IsPaletteDirty(const APCGExValencyAssetPalette* Palette) const
{
	return Palette && DirtyPalettes.Contains(const_cast<APCGExValencyAssetPalette*>(Palette));
}

bool FValencyDirtyStateManager::IsVolumeDirty(const AValencyContextVolume* Volume) const
{
	return Volume && DirtyVolumes.Contains(const_cast<AValencyContextVolume*>(Volume));
}

EValencyDirtyFlags FValencyDirtyStateManager::GetCageDirtyFlags(const APCGExValencyCageBase* Cage) const
{
	if (const EValencyDirtyFlags* Flags = DirtyCages.Find(const_cast<APCGExValencyCageBase*>(Cage)))
	{
		return *Flags;
	}
	return EValencyDirtyFlags::None;
}

int32 FValencyDirtyStateManager::ProcessDirty(bool bRebuildEnabled)
{
	if (bIsProcessing)
	{
		// Prevent recursive processing
		return 0;
	}

	if (!HasDirtyState())
	{
		return 0;
	}

	bIsProcessing = true;

	// Step 1: Expand dirty set through mirror relationships
	ExpandDirtyThroughMirrors();

	// Step 2: Refresh dirty cages/palettes (re-scan assets if needed)
	for (auto& Pair : DirtyCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(Pair.Key.Get()))
		{
			RefreshCageIfNeeded(Cage, Pair.Value);
		}
	}

	for (auto& Pair : DirtyPalettes)
	{
		if (APCGExValencyAssetPalette* Palette = Pair.Key.Get())
		{
			RefreshPaletteIfNeeded(Palette, Pair.Value);
		}
	}

	// Step 3: Collect all affected volumes
	TSet<AValencyContextVolume*> VolumesToRebuild;
	CollectAffectedVolumes(VolumesToRebuild);

	// Also add explicitly dirty volumes
	for (auto& Pair : DirtyVolumes)
	{
		if (AValencyContextVolume* Volume = Pair.Key.Get())
		{
			VolumesToRebuild.Add(Volume);
		}
	}

	// Step 4: Trigger rebuilds (if enabled and conditions met)
	int32 RebuildCount = 0;
	if (bRebuildEnabled && AValencyContextVolume::IsValencyModeActive())
	{
		for (AValencyContextVolume* Volume : VolumesToRebuild)
		{
			if (Volume && Volume->bAutoRebuildOnChange)
			{
				Volume->BuildRulesFromCages();
				RebuildCount++;

				// Only rebuild once per related volume set (BuildRulesFromCages finds related volumes)
				break;
			}
		}
	}

	// Step 5: Clear dirty state
	Reset();

	bIsProcessing = false;

	return RebuildCount;
}

void FValencyDirtyStateManager::ExpandDirtyThroughMirrors()
{
	if (!CachedCages)
	{
		return;
	}

	// Collect actors to check for mirroring (cages + palettes)
	TArray<AActor*> DirtyActors;
	for (auto& Pair : DirtyCages)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			DirtyActors.Add(Actor);
		}
	}
	for (auto& Pair : DirtyPalettes)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			DirtyActors.Add(Actor);
		}
	}

	// Find cages that mirror dirty actors
	TArray<APCGExValencyCage*> MirroringCages;
	for (AActor* DirtyActor : DirtyActors)
	{
		FindMirroringCages(DirtyActor, MirroringCages);
		for (APCGExValencyCage* MirroringCage : MirroringCages)
		{
			if (!DirtyCages.Contains(MirroringCage))
			{
				UE_LOG(LogTemp, Verbose, TEXT("Valency: Mirror cascade - marking cage '%s' dirty (mirrors dirty actor)"),
					*MirroringCage->GetCageDisplayName());
				MarkCageDirty(MirroringCage, EValencyDirtyFlags::Assets | EValencyDirtyFlags::MirrorSources);
			}
		}
	}
}

void FValencyDirtyStateManager::CollectAffectedVolumes(TSet<AValencyContextVolume*>& OutVolumes) const
{
	OutVolumes.Empty();

	if (!CachedVolumes)
	{
		return;
	}

	// Check each volume to see if it contains any dirty cage
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : *CachedVolumes)
	{
		AValencyContextVolume* Volume = VolumePtr.Get();
		if (!Volume)
		{
			continue;
		}

		// Check dirty cages
		for (auto& Pair : DirtyCages)
		{
			if (APCGExValencyCageBase* Cage = Pair.Key.Get())
			{
				if (Volume->ContainsPoint(Cage->GetActorLocation()))
				{
					OutVolumes.Add(Volume);
					break;
				}
			}
		}

		// Check dirty palettes (palettes can affect volumes through mirroring cages)
		// Note: Palettes themselves aren't in volumes, but cages that mirror them are
	}
}

void FValencyDirtyStateManager::FindMirroringCages(AActor* SourceActor, TArray<APCGExValencyCage*>& OutMirroringCages) const
{
	OutMirroringCages.Empty();

	if (!SourceActor || !CachedCages)
	{
		return;
	}

	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			if (Cage == SourceActor)
			{
				continue;
			}

			// Check if this cage mirrors the source actor
			for (const TObjectPtr<AActor>& MirrorSource : Cage->MirrorSources)
			{
				if (MirrorSource == SourceActor)
				{
					OutMirroringCages.Add(Cage);
					break;
				}
			}
		}
	}
}

void FValencyDirtyStateManager::RefreshCageIfNeeded(APCGExValencyCage* Cage, EValencyDirtyFlags Flags)
{
	if (!Cage)
	{
		return;
	}

	// Re-scan assets if asset-related flags are set
	if (EnumHasAnyFlags(Flags, EValencyDirtyFlags::Assets | EValencyDirtyFlags::Materials | EValencyDirtyFlags::Transform))
	{
		if (Cage->bAutoRegisterContainedAssets)
		{
			Cage->ScanAndRegisterContainedAssets();
		}
	}

	// Refresh orbital connections if orbital-related flags are set
	if (EnumHasAnyFlags(Flags, EValencyDirtyFlags::Orbitals | EValencyDirtyFlags::VolumeMembership))
	{
		Cage->RefreshContainingVolumes();
		Cage->DetectNearbyConnections();
	}

	// Refresh mirror ghost meshes only if mirror sources changed
	// (Not on Assets flag - that would cause constant refresh during asset dragging)
	if (EnumHasAnyFlags(Flags, EValencyDirtyFlags::MirrorSources))
	{
		Cage->RefreshMirrorGhostMeshes();
	}
}

void FValencyDirtyStateManager::RefreshPaletteIfNeeded(APCGExValencyAssetPalette* Palette, EValencyDirtyFlags Flags)
{
	if (!Palette)
	{
		return;
	}

	// Re-scan assets if asset-related flags are set
	if (EnumHasAnyFlags(Flags, EValencyDirtyFlags::Assets | EValencyDirtyFlags::Materials | EValencyDirtyFlags::Transform))
	{
		if (Palette->bAutoRegisterContainedAssets)
		{
			Palette->ScanAndRegisterContainedAssets();
		}
	}
}
