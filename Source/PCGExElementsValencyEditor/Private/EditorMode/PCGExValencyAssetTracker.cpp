// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyAssetTracker.h"

#include "Editor.h"
#include "Selection.h"

#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Volumes/ValencyContextVolume.h"

void FPCGExValencyAssetTracker::Initialize(
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>& InCachedCages,
	const TArray<TWeakObjectPtr<AValencyContextVolume>>& InCachedVolumes)
{
	CachedCages = &InCachedCages;
	CachedVolumes = &InCachedVolumes;
}

void FPCGExValencyAssetTracker::Reset()
{
	TrackedActors.Empty();
	TrackedActorCageMap.Empty();
	TrackedActorPositions.Empty();
}

bool FPCGExValencyAssetTracker::IsEnabled() const
{
	if (!CachedVolumes)
	{
		return false;
	}

	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : *CachedVolumes)
	{
		if (const AValencyContextVolume* Volume = VolumePtr.Get())
		{
			if (Volume->bAutoTrackAssetPlacement)
			{
				return true;
			}
		}
	}

	return false;
}

void FPCGExValencyAssetTracker::OnSelectionChanged()
{
	if (!IsEnabled())
	{
		return;
	}

	// Rebuild tracked actors list from current selection
	TrackedActors.Empty();

	if (!GEditor)
	{
		return;
	}

	USelection* Selection = GEditor->GetSelectedActors();
	if (!Selection)
	{
		return;
	}

	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor)
		{
			continue;
		}

		// Skip cages and volumes - we only track potential asset actors
		if (Cast<APCGExValencyCageBase>(Actor) || Cast<AValencyContextVolume>(Actor))
		{
			continue;
		}

		// Skip actors that are ignored by any volume with tracking enabled
		if (ShouldIgnoreActor(Actor))
		{
			continue;
		}

		TrackedActors.Add(Actor);

		// NOTE: Do NOT initialize position here - let Update() handle it
		// so that bIsNewActor can be properly detected for initial containment check
	}

	UE_LOG(LogTemp, Log, TEXT("Valency: Selection changed, tracking %d actors"), TrackedActors.Num());
}

bool FPCGExValencyAssetTracker::OnActorDeleted(AActor* DeletedActor)
{
	if (!DeletedActor || !IsEnabled())
	{
		return false;
	}

	// Check if this actor was in a tracked cage
	TWeakObjectPtr<APCGExValencyCage>* CagePtr = TrackedActorCageMap.Find(DeletedActor);
	if (CagePtr && CagePtr->IsValid())
	{
		APCGExValencyCage* ContainingCage = CagePtr->Get();
		UE_LOG(LogTemp, Log, TEXT("Valency: Tracked actor '%s' deleted from cage '%s'"),
			*DeletedActor->GetName(),
			*ContainingCage->GetCageDisplayName());

		// Refresh the cage
		ContainingCage->ScanAndRegisterContainedAssets();

		// Trigger auto-rebuild
		TSet<APCGExValencyCage*> AffectedCages;
		AffectedCages.Add(ContainingCage);
		TriggerAutoRebuild(AffectedCages);

		// Clean up tracking state
		TrackedActors.RemoveAll([DeletedActor](const TWeakObjectPtr<AActor>& ActorPtr)
		{
			return ActorPtr.Get() == DeletedActor || !ActorPtr.IsValid();
		});
		TrackedActorCageMap.Remove(DeletedActor);
		TrackedActorPositions.Remove(DeletedActor);

		return true;
	}

	// Clean up tracking state even if not in a cage
	TrackedActors.RemoveAll([DeletedActor](const TWeakObjectPtr<AActor>& ActorPtr)
	{
		return ActorPtr.Get() == DeletedActor || !ActorPtr.IsValid();
	});
	TrackedActorCageMap.Remove(DeletedActor);
	TrackedActorPositions.Remove(DeletedActor);

	return false;
}

bool FPCGExValencyAssetTracker::Update(TSet<APCGExValencyCage*>& OutAffectedCages)
{
	OutAffectedCages.Empty();

	if (TrackedActors.Num() == 0)
	{
		return false;
	}

	// Collect cages that can receive assets
	TArray<APCGExValencyCage*> TrackingCages;
	CollectTrackingCages(TrackingCages);

	if (TrackingCages.Num() == 0)
	{
		return false;
	}

	// Check each tracked actor
	for (int32 i = TrackedActors.Num() - 1; i >= 0; --i)
	{
		AActor* Actor = TrackedActors[i].Get();
		if (!Actor)
		{
			TrackedActors.RemoveAt(i);
			continue;
		}

		const FVector CurrentPos = Actor->GetActorLocation();

		// Check if this is a new actor or if position changed
		FVector* LastPos = TrackedActorPositions.Find(Actor);
		const bool bIsNewActor = (LastPos == nullptr);
		const bool bHasMoved = LastPos && FVector::DistSquared(*LastPos, CurrentPos) > KINDA_SMALL_NUMBER;

		// Update tracked position
		TrackedActorPositions.Add(Actor, CurrentPos);

		// Find which cage (if any) now contains this actor
		APCGExValencyCage* NewContainingCage = FindContainingCage(Actor);

		// Check if containment changed (or this is first check for a new actor)
		TWeakObjectPtr<APCGExValencyCage>* OldCagePtr = TrackedActorCageMap.Find(Actor);
		APCGExValencyCage* OldContainingCage = OldCagePtr ? OldCagePtr->Get() : nullptr;

		const bool bContainmentChanged = (NewContainingCage != OldContainingCage);
		const bool bNeedsInitialCheck = bIsNewActor && NewContainingCage != nullptr;

		// Also refresh if actor moved within a cage that preserves local transforms
		const bool bMovedWithinTransformCage = bHasMoved &&
			NewContainingCage &&
			NewContainingCage == OldContainingCage &&
			NewContainingCage->bPreserveLocalTransforms;

		if (bContainmentChanged || bNeedsInitialCheck || bMovedWithinTransformCage)
		{
			if (bMovedWithinTransformCage)
			{
				UE_LOG(LogTemp, Log, TEXT("Valency: Actor '%s' moved within cage '%s' (preserving local transforms)"),
					*Actor->GetName(),
					*NewContainingCage->GetCageDisplayName());
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("Valency: Actor '%s' containment changed - Old: %s, New: %s"),
					*Actor->GetName(),
					OldContainingCage ? *OldContainingCage->GetCageDisplayName() : TEXT("None"),
					NewContainingCage ? *NewContainingCage->GetCageDisplayName() : TEXT("None"));
			}

			// Containment changed - mark both cages for refresh
			if (OldContainingCage)
			{
				OutAffectedCages.Add(OldContainingCage);
			}
			if (NewContainingCage)
			{
				OutAffectedCages.Add(NewContainingCage);
			}

			// Update tracking map
			if (NewContainingCage)
			{
				TrackedActorCageMap.Add(Actor, NewContainingCage);
			}
			else
			{
				TrackedActorCageMap.Remove(Actor);
			}
		}
	}

	// Refresh affected cages
	for (APCGExValencyCage* Cage : OutAffectedCages)
	{
		if (Cage)
		{
			Cage->ScanAndRegisterContainedAssets();
			UE_LOG(LogTemp, Log, TEXT("Valency: Refreshed scanned assets for cage '%s'"), *Cage->GetCageDisplayName());
		}
	}

	return OutAffectedCages.Num() > 0;
}

void FPCGExValencyAssetTracker::TriggerAutoRebuild(const TSet<APCGExValencyCage*>& AffectedCages)
{
	if (!CachedVolumes || AffectedCages.Num() == 0)
	{
		return;
	}

	// Expand affected cages to include cages that mirror them (mirror cascade)
	TSet<APCGExValencyCage*> ExpandedAffectedCages = AffectedCages;

	for (APCGExValencyCage* AffectedCage : AffectedCages)
	{
		if (AffectedCage)
		{
			TArray<APCGExValencyCage*> MirroringCages;
			FindCagesThatMirror(AffectedCage, MirroringCages);

			for (APCGExValencyCage* MirroringCage : MirroringCages)
			{
				if (MirroringCage && !ExpandedAffectedCages.Contains(MirroringCage))
				{
					UE_LOG(LogTemp, Log, TEXT("Valency: Mirror cascade - Cage '%s' mirrors affected cage '%s', adding to rebuild"),
						*MirroringCage->GetCageDisplayName(),
						*AffectedCage->GetCageDisplayName());
					ExpandedAffectedCages.Add(MirroringCage);
				}
			}
		}
	}

	TSet<AValencyContextVolume*> VolumesToRebuild;

	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : *CachedVolumes)
	{
		if (AValencyContextVolume* Volume = VolumePtr.Get())
		{
			if (Volume->bAutoRebuildOnChange)
			{
				// Check if any affected cage is in this volume
				for (APCGExValencyCage* Cage : ExpandedAffectedCages)
				{
					if (Cage && Volume->ContainsPoint(Cage->GetActorLocation()))
					{
						VolumesToRebuild.Add(Volume);
						break;
					}
				}
			}
		}
	}

	for (AValencyContextVolume* Volume : VolumesToRebuild)
	{
		UE_LOG(LogTemp, Log, TEXT("Valency: Auto-rebuilding rules for volume"));
		Volume->BuildRulesFromCages();
	}
}

bool FPCGExValencyAssetTracker::ShouldIgnoreActor(AActor* Actor) const
{
	if (!Actor || !CachedVolumes)
	{
		return true;
	}

	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : *CachedVolumes)
	{
		if (const AValencyContextVolume* Volume = VolumePtr.Get())
		{
			if (Volume->bAutoTrackAssetPlacement && Volume->ShouldIgnoreActor(Actor))
			{
				return true;
			}
		}
	}

	return false;
}

APCGExValencyCage* FPCGExValencyAssetTracker::FindContainingCage(AActor* Actor) const
{
	if (!Actor || !CachedCages)
	{
		return nullptr;
	}

	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			if (!Cage->IsNullCage() && Cage->IsActorInside(Actor))
			{
				return Cage;
			}
		}
	}

	return nullptr;
}

void FPCGExValencyAssetTracker::CollectTrackingCages(TArray<APCGExValencyCage*>& OutCages) const
{
	OutCages.Empty();

	if (!CachedCages)
	{
		return;
	}

	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			if (!Cage->IsNullCage())
			{
				OutCages.Add(Cage);
			}
		}
	}
}

void FPCGExValencyAssetTracker::FindCagesThatMirror(APCGExValencyCage* SourceCage, TArray<APCGExValencyCage*>& OutMirroringCages) const
{
	OutMirroringCages.Empty();

	if (!SourceCage || !CachedCages)
	{
		return;
	}

	// Find all cages that have SourceCage in their MirrorSources
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
			if (Cage == SourceCage)
			{
				continue; // Skip the source cage itself
			}

			// Check if this cage mirrors the source cage
			for (const TObjectPtr<AActor>& MirrorSource : Cage->MirrorSources)
			{
				if (MirrorSource == SourceCage)
				{
					OutMirroringCages.Add(Cage);
					break;
				}
			}
		}
	}
}
