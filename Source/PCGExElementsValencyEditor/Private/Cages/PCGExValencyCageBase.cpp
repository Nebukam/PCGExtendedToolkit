// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageBase.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Volumes/ValencyContextVolume.h"
#include "Cages/PCGExValencyCageSpatialRegistry.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace PCGExValencyFolders
{
	const FName CagesFolder = FName(TEXT("Valency/Cages"));
	const FName VolumesFolder = FName(TEXT("Valency/Volumes"));
}

APCGExValencyCageBase::APCGExValencyCageBase()
{
	// Configure as editor-only
	bNetLoadOnClient = false;
	bReplicates = false;

	// Default root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

void APCGExValencyCageBase::PostActorCreated()
{
	Super::PostActorCreated();

	// Mark as newly created (not loaded from disk)
	bIsNewlyCreated = true;

	// Auto-organize into Valency/Cages folder
	SetFolderPath(PCGExValencyFolders::CagesFolder);
}

void APCGExValencyCageBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Register with spatial registry
	if (UWorld* World = GetWorld())
	{
		FPCGExValencyCageSpatialRegistry::Get(World).RegisterCage(this);
	}

	// Initial setup
	RefreshContainingVolumes();

	if (bNeedsOrbitalInit)
	{
		InitializeOrbitalsFromSet();
		bNeedsOrbitalInit = false;
	}

	// Initialize drag tracking
	LastDragUpdatePosition = GetActorLocation();

	// If this is a newly created cage (not loaded), trigger auto-rebuild for containing volumes
	if (bIsNewlyCreated)
	{
		bIsNewlyCreated = false; // Clear flag
		TriggerAutoRebuildIfNeeded();
	}
}

void APCGExValencyCageBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, OrbitalSetOverride) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, BondingRulesOverride))
	{
		// Override changed - reinitialize orbitals and redraw
		CachedOrbitalSet.Reset();
		InitializeOrbitalsFromSet();
		DetectNearbyConnections();

#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
#endif
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, ProbeRadius))
	{
		// Probe radius changed - redetect connections and redraw
		DetectNearbyConnections();

#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
#endif
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, CageName))
	{
		// Display name changed - just redraw
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
#endif
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, bTransformOrbitalDirections))
	{
		// Transform settings changed - redetect connections and redraw
		DetectNearbyConnections();

#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->RedrawAllViewports();
		}
#endif
	}
}

void APCGExValencyCageBase::BeginDestroy()
{
	// Unregister from spatial registry and trigger auto-rebuild if this is a user deletion
	if (UWorld* World = GetWorld())
	{
		FPCGExValencyCageSpatialRegistry::Get(World).UnregisterCage(this);

		// Only trigger auto-rebuild if world is valid and not being torn down
#if WITH_EDITOR
		if (!World->bIsTearingDown && !World->IsPlayInEditor() && ContainingVolumes.Num() > 0)
		{
			TriggerAutoRebuildIfNeeded();
		}
#endif
	}

	Super::BeginDestroy();
}

void APCGExValencyCageBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	const FVector CurrentPosition = GetActorLocation();

	if (!bFinished)
	{
		// Continuous drag - track state and do throttled live updates
		if (!bIsDragging)
		{
			// Drag just started - capture current state
			bIsDragging = true;
			DragStartPosition = LastDragUpdatePosition;
			VolumesBeforeDrag = ContainingVolumes; // Capture volume membership before drag
		}

		// Check if we've moved enough to warrant an update
		const float DistanceMoved = FVector::Dist(CurrentPosition, LastDragUpdatePosition);
		if (DistanceMoved >= DragUpdateThreshold)
		{
			UpdateConnectionsDuringDrag();
			LastDragUpdatePosition = CurrentPosition;
		}
	}
	else
	{
		// Drag finished - do final update
		const bool bWasDragging = bIsDragging;
		bIsDragging = false;

		// Update spatial registry with final position
		if (UWorld* World = GetWorld())
		{
			FPCGExValencyCageSpatialRegistry::Get(World).UpdateCagePosition(this, DragStartPosition, CurrentPosition);
		}

		// Capture old volumes before refresh (only valid if we had a proper drag sequence)
		TArray<TWeakObjectPtr<AValencyContextVolume>> OldVolumes = VolumesBeforeDrag;

		// Position changed - refresh volumes and connections
		RefreshContainingVolumes();
		DetectNearbyConnections();

		// Check for volume membership changes and trigger auto-rebuild if needed
		// Only do this if we properly captured the "before" state during drag start
		if (bWasDragging)
		{
			HandleVolumeMembershipChange(OldVolumes);
		}

		// Always trigger rebuild after movement - connections may have changed
		// HandleVolumeMembershipChange only covers in/out of volumes, not within-volume moves
		TriggerAutoRebuildIfNeeded();

		// Notify affected cages using spatial registry for efficiency
		NotifyAffectedCagesOfMovement(DragStartPosition, CurrentPosition);

		LastDragUpdatePosition = CurrentPosition;
		VolumesBeforeDrag.Empty(); // Clear after use
	}
}

FString APCGExValencyCageBase::GetCageDisplayName() const
{
	if (!CageName.IsEmpty())
	{
		return CageName;
	}

	return GetActorNameOrLabel();
}

UPCGExValencyOrbitalSet* APCGExValencyCageBase::GetEffectiveOrbitalSet() const
{
	// Check override first
	if (OrbitalSetOverride)
	{
		return OrbitalSetOverride;
	}

	// Check containing volumes
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
	{
		if (const AValencyContextVolume* Volume = VolumePtr.Get())
		{
			if (UPCGExValencyOrbitalSet* Set = Volume->GetEffectiveOrbitalSet())
			{
				return Set;
			}
		}
	}

	return nullptr;
}

UPCGExValencyBondingRules* APCGExValencyCageBase::GetEffectiveBondingRules() const
{
	// Check override first
	if (BondingRulesOverride)
	{
		return BondingRulesOverride;
	}

	// Check containing volumes
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
	{
		if (const AValencyContextVolume* Volume = VolumePtr.Get())
		{
			if (UPCGExValencyBondingRules* Rules = Volume->GetBondingRules())
			{
				return Rules;
			}
		}
	}

	return nullptr;
}

float APCGExValencyCageBase::GetEffectiveProbeRadius() const
{
	float BaseRadius;

	// Explicit override
	if (ProbeRadius >= 0.0f)
	{
		BaseRadius = ProbeRadius;
	}
	else
	{
		// Get from first containing volume
		BaseRadius = 100.0f; // Default fallback
		for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
		{
			if (const AValencyContextVolume* Volume = VolumePtr.Get())
			{
				BaseRadius = Volume->GetDefaultProbeRadius();
				break;
			}
		}
	}

	return BaseRadius;
}

bool APCGExValencyCageBase::ShouldTransformOrbitalDirections() const
{
	return bTransformOrbitalDirections;
}

bool APCGExValencyCageBase::HasConnectionTo(const APCGExValencyCageBase* OtherCage) const
{
	if (!OtherCage)
	{
		return false;
	}

	for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		if (!Orbital.bEnabled)
		{
			continue;
		}

		// Check manual connections list
		if (Orbital.IsManualTarget(OtherCage))
		{
			return true;
		}

		// Check auto connection
		if (Orbital.AutoConnectedCage.Get() == OtherCage)
		{
			return true;
		}
	}

	return false;
}

int32 APCGExValencyCageBase::GetOrbitalIndexTo(const APCGExValencyCageBase* OtherCage) const
{
	if (!OtherCage)
	{
		return -1;
	}

	for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		if (!Orbital.bEnabled)
		{
			continue;
		}

		// Check manual connections list
		if (Orbital.IsManualTarget(OtherCage))
		{
			return Orbital.OrbitalIndex;
		}

		// Check auto connection
		if (Orbital.AutoConnectedCage.Get() == OtherCage)
		{
			return Orbital.OrbitalIndex;
		}
	}

	return -1;
}

void APCGExValencyCageBase::OnContainingVolumeChanged(AValencyContextVolume* Volume)
{
	// Refresh our state when a containing volume changes
	RefreshContainingVolumes();

	// If orbital set changed, reinitialize
	UPCGExValencyOrbitalSet* NewSet = GetEffectiveOrbitalSet();
	if (NewSet != CachedOrbitalSet.Get())
	{
		CachedOrbitalSet = NewSet;
		InitializeOrbitalsFromSet();
	}
}

void APCGExValencyCageBase::RefreshContainingVolumes()
{
	ContainingVolumes.Empty();

	if (UWorld* World = GetWorld())
	{
		const FVector MyLocation = GetActorLocation();

		for (TActorIterator<AValencyContextVolume> It(World); It; ++It)
		{
			AValencyContextVolume* Volume = *It;
			if (Volume && Volume->ContainsPoint(MyLocation))
			{
				ContainingVolumes.Add(Volume);
			}
		}
	}
}

bool APCGExValencyCageBase::ShouldIgnoreActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return true;
	}

	// Check against all containing volumes' ignore rules
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
	{
		if (const AValencyContextVolume* Volume = VolumePtr.Get())
		{
			if (Volume->ShouldIgnoreActor(Actor))
			{
				return true;
			}
		}
	}

	return false;
}

void APCGExValencyCageBase::InitializeOrbitalsFromSet()
{
	const UPCGExValencyOrbitalSet* OrbitalSet = GetEffectiveOrbitalSet();
	if (!OrbitalSet)
	{
		// No orbital set - clear orbitals
		Orbitals.Empty();
		return;
	}

	// Cache the set
	CachedOrbitalSet = const_cast<UPCGExValencyOrbitalSet*>(OrbitalSet);

	// Preserve existing data where possible
	TMap<int32, TArray<TObjectPtr<APCGExValencyCageBase>>> ExistingManualConnections;
	TMap<int32, TWeakObjectPtr<APCGExValencyCageBase>> ExistingAutoConnections;
	TMap<int32, bool> ExistingEnabled;

	for (const FPCGExValencyCageOrbital& Existing : Orbitals)
	{
		if (Existing.OrbitalIndex >= 0)
		{
			ExistingManualConnections.Add(Existing.OrbitalIndex, Existing.ManualConnections);
			ExistingAutoConnections.Add(Existing.OrbitalIndex, Existing.AutoConnectedCage);
			ExistingEnabled.Add(Existing.OrbitalIndex, Existing.bEnabled);
		}
	}

	// Rebuild orbitals array from OrbitalSet
	Orbitals.Empty(OrbitalSet->Num());

	for (int32 i = 0; i < OrbitalSet->Num(); ++i)
	{
		const FPCGExValencyOrbitalEntry& Entry = OrbitalSet->Orbitals[i];

		FPCGExValencyCageOrbital& Orbital = Orbitals.AddDefaulted_GetRef();
		Orbital.OrbitalIndex = i;
		Orbital.OrbitalName = Entry.GetOrbitalName();

		// Restore existing manual connections if any (user-defined, persisted)
		if (TArray<TObjectPtr<APCGExValencyCageBase>>* ManualPtr = ExistingManualConnections.Find(i))
		{
			Orbital.ManualConnections = *ManualPtr;
		}
		// Restore existing auto connection if any (will be refreshed by DetectNearbyConnections)
		if (TWeakObjectPtr<APCGExValencyCageBase>* AutoPtr = ExistingAutoConnections.Find(i))
		{
			Orbital.AutoConnectedCage = *AutoPtr;
		}
		if (bool* EnabledPtr = ExistingEnabled.Find(i))
		{
			Orbital.bEnabled = *EnabledPtr;
		}
	}
}

void APCGExValencyCageBase::DetectNearbyConnections()
{
	const float Radius = GetEffectiveProbeRadius();
	if (Radius <= 0.0f)
	{
		// Radius 0 = receive-only, don't detect
		return;
	}

	const UPCGExValencyOrbitalSet* OrbitalSet = GetEffectiveOrbitalSet();
	if (!OrbitalSet || OrbitalSet->Num() == 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector MyLocation = GetActorLocation();
	const FTransform MyTransform = GetActorTransform();

	// Build orbital cache for direction matching
	PCGExValency::FOrbitalCache OrbitalCache;
	if (!OrbitalCache.BuildFrom(OrbitalSet))
	{
		return;
	}

	// Build set of cages that are manual targets (excluded from auto-detection)
	TSet<const APCGExValencyCageBase*> ManualTargets;
	for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		for (const TObjectPtr<APCGExValencyCageBase>& ManualCage : Orbital.ManualConnections)
		{
			if (ManualCage)
			{
				ManualTargets.Add(ManualCage);
			}
		}
	}

	// Clear existing auto-connections (manual connections are preserved)
	for (FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		Orbital.AutoConnectedCage = nullptr;
	}

	// Find nearby cages
	for (TActorIterator<APCGExValencyCageBase> It(World); It; ++It)
	{
		APCGExValencyCageBase* OtherCage = *It;
		if (OtherCage == this)
		{
			continue;
		}

		// Skip cages that are manual targets - they're handled separately
		if (ManualTargets.Contains(OtherCage))
		{
			continue;
		}

		const FVector OtherLocation = OtherCage->GetActorLocation();
		const float Distance = FVector::Dist(MyLocation, OtherLocation);

		if (Distance > Radius)
		{
			continue;
		}

		// Check direction to other cage
		const FVector Direction = (OtherLocation - MyLocation).GetSafeNormal();

		// Find matching orbital (use per-cage transform setting)
		const uint8 OrbitalIndex = OrbitalCache.FindMatchingOrbital(
			Direction,
			ShouldTransformOrbitalDirections(),
			MyTransform
		);

		if (OrbitalIndex != PCGExValency::NO_ORBITAL_MATCH && Orbitals.IsValidIndex(OrbitalIndex))
		{
			FPCGExValencyCageOrbital& Orbital = Orbitals[OrbitalIndex];
			if (!Orbital.AutoConnectedCage.IsValid())
			{
				// Connect to closest cage in this direction (auto-detected)
				Orbital.AutoConnectedCage = OtherCage;
			}
		}
	}
}

int32 APCGExValencyCageBase::CleanupManualConnections()
{
	int32 TotalRemoved = 0;

	for (FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		TotalRemoved += Orbital.CleanupManualConnections();
	}

	if (TotalRemoved > 0)
	{
		Modify(); // Mark as needing save
	}

	return TotalRemoved;
}

void APCGExValencyCageBase::OnRelatedCageMoved(APCGExValencyCageBase* MovedCage)
{
	if (!MovedCage || MovedCage == this)
	{
		return;
	}

	// The spatial registry pre-filters to only call us if we're potentially affected,
	// so we can directly refresh our connections
	DetectNearbyConnections();
}

void APCGExValencyCageBase::NotifyAllCagesOfMovement()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Notify all other cages that we moved
	for (TActorIterator<APCGExValencyCageBase> It(World); It; ++It)
	{
		APCGExValencyCageBase* OtherCage = *It;
		if (OtherCage && OtherCage != this)
		{
			OtherCage->OnRelatedCageMoved(this);
		}
	}

#if WITH_EDITOR
	// Request viewport redraw to show updated connections
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}
#endif
}

void APCGExValencyCageBase::SetDebugComponentsVisible(bool bVisible)
{
	// Base implementation does nothing - subclasses override to hide their specific components
}

void APCGExValencyCageBase::UpdateConnectionsDuringDrag()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector CurrentPosition = GetActorLocation();
	FPCGExValencyCageSpatialRegistry& Registry = FPCGExValencyCageSpatialRegistry::Get(World);

	// Find cages that might be affected by our movement
	TSet<APCGExValencyCageBase*> AffectedCages;
	Registry.FindAffectedCages(this, LastDragUpdatePosition, CurrentPosition, AffectedCages);

	// Update our own connections
	DetectNearbyConnections();

	// Update affected cages' connections
	for (APCGExValencyCageBase* Cage : AffectedCages)
	{
		if (Cage)
		{
			Cage->DetectNearbyConnections();
		}
	}

#if WITH_EDITOR
	// Request viewport redraw for live feedback
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}
#endif
}

void APCGExValencyCageBase::NotifyAffectedCagesOfMovement(const FVector& OldPosition, const FVector& NewPosition)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FPCGExValencyCageSpatialRegistry& Registry = FPCGExValencyCageSpatialRegistry::Get(World);

	// Find all cages affected by our movement
	TSet<APCGExValencyCageBase*> AffectedCages;
	Registry.FindAffectedCages(this, OldPosition, NewPosition, AffectedCages);

	// Notify each affected cage
	for (APCGExValencyCageBase* Cage : AffectedCages)
	{
		if (Cage)
		{
			Cage->OnRelatedCageMoved(this);
		}
	}

#if WITH_EDITOR
	// Request viewport redraw to show updated connections
	if (GEditor)
	{
		GEditor->RedrawAllViewports();
	}
#endif
}

void APCGExValencyCageBase::HandleVolumeMembershipChange(const TArray<TWeakObjectPtr<AValencyContextVolume>>& OldVolumes)
{
	// Only process auto-rebuild when Valency mode is active
	if (!AValencyContextVolume::IsValencyModeActive())
	{
		return;
	}

	// Build sets for comparison
	TSet<AValencyContextVolume*> OldVolumeSet;
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : OldVolumes)
	{
		if (AValencyContextVolume* Volume = VolumePtr.Get())
		{
			OldVolumeSet.Add(Volume);
		}
	}

	TSet<AValencyContextVolume*> NewVolumeSet;
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
	{
		if (AValencyContextVolume* Volume = VolumePtr.Get())
		{
			NewVolumeSet.Add(Volume);
		}
	}

	// Find volumes affected by membership change
	TArray<AValencyContextVolume*> AffectedVolumes;

	// Volumes that lost this cage (was in old, not in new)
	for (AValencyContextVolume* OldVolume : OldVolumeSet)
	{
		if (!NewVolumeSet.Contains(OldVolume))
		{
			AffectedVolumes.AddUnique(OldVolume);
		}
	}

	// Volumes that gained this cage (in new, not in old)
	for (AValencyContextVolume* NewVolume : NewVolumeSet)
	{
		if (!OldVolumeSet.Contains(NewVolume))
		{
			AffectedVolumes.AddUnique(NewVolume);
		}
	}

	// Trigger rebuild for affected volumes
	TriggerAutoRebuildForVolumes(AffectedVolumes);
}

bool APCGExValencyCageBase::TriggerAutoRebuildIfNeeded()
{
	// Only process when Valency mode is active
	if (!AValencyContextVolume::IsValencyModeActive())
	{
		return false;
	}

	// Collect containing volumes
	TArray<AValencyContextVolume*> Volumes;
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
	{
		if (AValencyContextVolume* Volume = VolumePtr.Get())
		{
			Volumes.Add(Volume);
		}
	}

	return TriggerAutoRebuildForVolumes(Volumes);
}

bool APCGExValencyCageBase::TriggerAutoRebuildForVolumes(const TArray<AValencyContextVolume*>& Volumes)
{
	// Only process when Valency mode is active
	if (!AValencyContextVolume::IsValencyModeActive())
	{
		return false;
	}

	// Find first volume with auto-rebuild enabled and trigger it
	// Multi-volume aggregation handles rebuilding all related volumes
	for (AValencyContextVolume* Volume : Volumes)
	{
		if (Volume && Volume->bAutoRebuildOnChange)
		{
			Volume->BuildRulesFromCages();
			return true;
		}
	}

	return false;
}
