// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageBase.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "Volumes/ValencyContextVolume.h"

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

	// Initial setup
	RefreshContainingVolumes();

	if (bNeedsOrbitalInit)
	{
		InitializeOrbitalsFromSet();
		bNeedsOrbitalInit = false;
	}
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

void APCGExValencyCageBase::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		// Position changed - refresh volumes and connections
		RefreshContainingVolumes();
		DetectNearbyConnections();
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
		if (Orbital.bEnabled && Orbital.ConnectedCage.Get() == OtherCage)
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
		if (Orbital.bEnabled && Orbital.ConnectedCage.Get() == OtherCage)
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
	TMap<int32, TWeakObjectPtr<APCGExValencyCageBase>> ExistingConnections;
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
		if (TWeakObjectPtr<APCGExValencyCageBase>* ExistingPtr = ExistingConnections.Find(i))
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
		Orbital.ConnectedCage.Reset();
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
			if (!Orbital.ConnectedCage.IsValid())
			{
				// Connect to closest cage in this direction
				Orbital.ConnectedCage = OtherCage;
			}
		}
	}
}
