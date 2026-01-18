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
}

void APCGExValencyCageBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, OrbitalSetOverride) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, BondingRulesOverride))
	{
		// Override changed - reinitialize orbitals
		CachedOrbitalSet.Reset();
		InitializeOrbitalsFromSet();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageBase, ProbeRadius))
	{
		// Probe radius changed - redetect connections
		DetectNearbyConnections();
	}
}

void APCGExValencyCageBase::BeginDestroy()
{
	// Unregister from spatial registry
	if (UWorld* World = GetWorld())
	{
		FPCGExValencyCageSpatialRegistry::Get(World).UnregisterCage(this);
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
			// Drag just started
			bIsDragging = true;
			DragStartPosition = LastDragUpdatePosition;
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
		bIsDragging = false;

		// Update spatial registry with final position
		if (UWorld* World = GetWorld())
		{
			FPCGExValencyCageSpatialRegistry::Get(World).UpdateCagePosition(this, DragStartPosition, CurrentPosition);
		}

		// Position changed - refresh volumes and connections
		RefreshContainingVolumes();
		DetectNearbyConnections();

		// Notify affected cages using spatial registry for efficiency
		NotifyAffectedCagesOfMovement(DragStartPosition, CurrentPosition);

		LastDragUpdatePosition = CurrentPosition;
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
	// Explicit override
	if (ProbeRadius >= 0.0f)
	{
		return ProbeRadius;
	}

	// Get from first containing volume
	for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : ContainingVolumes)
	{
		if (const AValencyContextVolume* Volume = VolumePtr.Get())
		{
			return Volume->GetDefaultProbeRadius();
		}
	}

	// Default fallback
	return 100.0f;
}

bool APCGExValencyCageBase::HasConnectionTo(const APCGExValencyCageBase* OtherCage) const
{
	if (!OtherCage)
	{
		return false;
	}

	for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		if (Orbital.bEnabled && Orbital.ConnectedCage == OtherCage)
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
		if (Orbital.bEnabled && Orbital.ConnectedCage == OtherCage)
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

	// Preserve existing connections where possible
	TMap<int32, TObjectPtr<APCGExValencyCageBase>> ExistingConnections;
	TMap<int32, bool> ExistingEnabled;

	for (const FPCGExValencyCageOrbital& Existing : Orbitals)
	{
		if (Existing.OrbitalIndex >= 0)
		{
			ExistingConnections.Add(Existing.OrbitalIndex, Existing.ConnectedCage);
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

		// Restore existing connection if any
		if (TObjectPtr<APCGExValencyCageBase>* ExistingPtr = ExistingConnections.Find(i))
		{
			Orbital.ConnectedCage = *ExistingPtr;
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

	// Clear existing connections
	for (FPCGExValencyCageOrbital& Orbital : Orbitals)
	{
		Orbital.ConnectedCage = nullptr;
	}

	// Find nearby cages
	for (TActorIterator<APCGExValencyCageBase> It(World); It; ++It)
	{
		APCGExValencyCageBase* OtherCage = *It;
		if (OtherCage == this)
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

		// Find matching orbital
		const uint8 OrbitalIndex = OrbitalCache.FindMatchingOrbital(
			Direction,
			OrbitalSet->bTransformDirection,
			MyTransform
		);

		if (OrbitalIndex != PCGExValency::NO_ORBITAL_MATCH && Orbitals.IsValidIndex(OrbitalIndex))
		{
			FPCGExValencyCageOrbital& Orbital = Orbitals[OrbitalIndex];
			if (!Orbital.ConnectedCage)
			{
				// Connect to closest cage in this direction
				Orbital.ConnectedCage = OtherCage;
			}
		}
	}
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
