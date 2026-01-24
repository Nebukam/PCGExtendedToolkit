// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyDirtyState.h"
#include "EditorMode/PCGExValencyReferenceTracker.h"

#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCagePattern.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"

void FValencyDirtyStateManager::Initialize(
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& InCachedCages,
	const TArray<TWeakObjectPtr<AValencyContextVolume>>& InCachedVolumes,
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>& InCachedPalettes,
	FValencyReferenceTracker* InReferenceTracker)
{
	CachedCages = &InCachedCages;
	CachedVolumes = &InCachedVolumes;
	CachedPalettes = &InCachedPalettes;
	ReferenceTracker = InReferenceTracker;
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

	// Step 1: Expand dirty set through transitive dependencies (via ReferenceTracker)
	ExpandDirtyThroughDependencies();

	// Step 2: Refresh dirty cages/palettes (re-scan assets if needed)
	for (auto& Pair : DirtyCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(Pair.Key.Get()))
		{
			RefreshCageIfNeeded(Cage, Pair.Value);
		}
		else if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(Pair.Key.Get()))
		{
			// Pattern cages don't have assets to scan, but need orbital connection refresh
			if (EnumHasAnyFlags(Pair.Value, EValencyDirtyFlags::Orbitals | EValencyDirtyFlags::VolumeMembership | EValencyDirtyFlags::Structure))
			{
				PatternCage->RefreshContainingVolumes();
				PatternCage->DetectNearbyConnections();
			}
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

void FValencyDirtyStateManager::ExpandDirtyThroughDependencies()
{
	if (!ReferenceTracker)
	{
		// No reference tracker - cannot expand transitively
		return;
	}

	// Collect all currently dirty actors
	TSet<AActor*> OriginalDirty;
	GetAllDirtyActors(OriginalDirty);

	if (OriginalDirty.Num() == 0)
	{
		return;
	}

	// Use ReferenceTracker to find all transitively affected actors
	TSet<AActor*> AllAffected;
	for (AActor* DirtyActor : OriginalDirty)
	{
		// Get all dependents (actors that depend on this dirty actor)
		if (const TArray<TWeakObjectPtr<AActor>>* Dependents = ReferenceTracker->GetDependents(DirtyActor))
		{
			for (const TWeakObjectPtr<AActor>& DepPtr : *Dependents)
			{
				if (AActor* Dependent = DepPtr.Get())
				{
					AllAffected.Add(Dependent);
				}
			}
		}
	}

	// BFS to find transitive dependents
	TArray<AActor*> ToProcess = AllAffected.Array();
	while (ToProcess.Num() > 0)
	{
		AActor* Current = ToProcess.Pop(EAllowShrinking::No);

		if (const TArray<TWeakObjectPtr<AActor>>* Dependents = ReferenceTracker->GetDependents(Current))
		{
			for (const TWeakObjectPtr<AActor>& DepPtr : *Dependents)
			{
				if (AActor* Dependent = DepPtr.Get())
				{
					if (!AllAffected.Contains(Dependent) && !OriginalDirty.Contains(Dependent))
					{
						AllAffected.Add(Dependent);
						ToProcess.Add(Dependent);
					}
				}
			}
		}
	}

	// Mark all affected actors as dirty
	for (AActor* Affected : AllAffected)
	{
		if (OriginalDirty.Contains(Affected))
		{
			continue; // Already dirty
		}

		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(Affected))
		{
			MarkCageDirty(Cage, EValencyDirtyFlags::Structure);
		}
		else if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(Affected))
		{
			MarkCageDirty(PatternCage, EValencyDirtyFlags::Structure);
		}
	}
}

void FValencyDirtyStateManager::GetAllDirtyActors(TSet<AActor*>& OutDirtyActors) const
{
	OutDirtyActors.Empty();

	for (const auto& Pair : DirtyCages)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			OutDirtyActors.Add(Actor);
		}
	}

	for (const auto& Pair : DirtyPalettes)
	{
		if (AActor* Actor = Pair.Key.Get())
		{
			OutDirtyActors.Add(Actor);
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

		bool bVolumeAffected = false;

		// Check dirty cages
		for (auto& Pair : DirtyCages)
		{
			if (bVolumeAffected)
			{
				break;
			}

			if (APCGExValencyCageBase* Cage = Pair.Key.Get())
			{
				// Direct containment check
				if (Volume->ContainsPoint(Cage->GetActorLocation()))
				{
					bVolumeAffected = true;
					break;
				}

				// For pattern cages, check the entire connected pattern network
				if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(Cage))
				{
					// Check all connected pattern cages (includes self)
					TArray<APCGExValencyCagePattern*> ConnectedCages = PatternCage->GetConnectedPatternCages();
					for (APCGExValencyCagePattern* Connected : ConnectedCages)
					{
						if (Connected && Volume->ContainsPoint(Connected->GetActorLocation()))
						{
							bVolumeAffected = true;
							break;
						}
					}
				}
			}
		}

		if (bVolumeAffected)
		{
			OutVolumes.Add(Volume);
		}

		// Check dirty palettes (palettes can affect volumes through mirroring cages)
		// Note: Palettes themselves aren't in volumes, but cages that mirror them are
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
