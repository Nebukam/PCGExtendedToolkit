// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageNull.h"
#include "Cages/PCGExValencyCageSpatialRegistry.h"

#include "Components/SphereComponent.h"
#include "Volumes/ValencyContextVolume.h"

APCGExValencyCageNull::APCGExValencyCageNull()
{
	// Create a small sphere for visualization and selection
	DebugSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("DebugSphere"));
	DebugSphereComponent->SetupAttachment(RootComponent);
	DebugSphereComponent->SetSphereRadius(15.0f);
	DebugSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugSphereComponent->SetLineThickness(2.0f);
	DebugSphereComponent->ShapeColor = FColor(255, 100, 100); // Default: Boundary (red)
	DebugSphereComponent->bHiddenInGame = true;
}

void APCGExValencyCageNull::PostEditMove(bool bFinished)
{
	// Let base class handle volume membership, connection updates, and rebuild triggering
	// Base class already calls RequestRebuild(Movement) when drag finishes
	Super::PostEditMove(bFinished);
}

#if WITH_EDITOR
void APCGExValencyCageNull::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageNull, PlaceholderMode))
	{
		// Mode changed - update visualization and trigger rebuild
		UpdateVisualization();

		// Re-detect connections since our participation state may have changed
		DetectNearbyConnections();

		RequestRebuild(EValencyRebuildReason::PropertyChange);
	}
}
#endif

FString APCGExValencyCageNull::GetCageDisplayName() const
{
	// Build mode prefix
	FString ModePrefix;
	switch (PlaceholderMode)
	{
	case EPCGExPlaceholderMode::Boundary:
		ModePrefix = TEXT("BOUNDARY");
		break;
	case EPCGExPlaceholderMode::Wildcard:
		ModePrefix = TEXT("WILDCARD");
		break;
	case EPCGExPlaceholderMode::Any:
		ModePrefix = TEXT("ANY");
		break;
	}

	if (!CageName.IsEmpty())
	{
		return FString::Printf(TEXT("%s: %s"), *ModePrefix, *CageName);
	}

	if (!Description.IsEmpty())
	{
		return FString::Printf(TEXT("%s (%s)"), *ModePrefix, *Description);
	}

	return FString::Printf(TEXT("%s Cage"), *ModePrefix);
}

void APCGExValencyCageNull::SetDebugComponentsVisible(bool bVisible)
{
	if (DebugSphereComponent)
	{
		DebugSphereComponent->SetVisibility(bVisible);
	}
}

void APCGExValencyCageNull::UpdateVisualization()
{
	if (!DebugSphereComponent)
	{
		return;
	}

	// Update color based on mode
	switch (PlaceholderMode)
	{
	case EPCGExPlaceholderMode::Boundary:
		DebugSphereComponent->ShapeColor = FColor(255, 100, 100); // Red
		break;
	case EPCGExPlaceholderMode::Wildcard:
		DebugSphereComponent->ShapeColor = FColor(200, 50, 200); // Magenta
		break;
	case EPCGExPlaceholderMode::Any:
		DebugSphereComponent->ShapeColor = FColor(100, 200, 255); // Cyan
		break;
	}

	// Force visual refresh
	DebugSphereComponent->MarkRenderStateDirty();
}

bool APCGExValencyCageNull::HasNearbyPatternCages() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float Radius = GetEffectiveProbeRadius();
	if (Radius <= 0.0f)
	{
		// If this cage has no probe radius, check if any pattern cage can reach us
		// Use the spatial registry's max probe radius
		const float MaxRadius = FPCGExValencyCageSpatialRegistry::Get(World).GetMaxProbeRadius();
		if (MaxRadius <= 0.0f)
		{
			return false;
		}
	}

	const FVector MyLocation = GetActorLocation();

	// Use spatial registry to find nearby cages
	TArray<APCGExValencyCageBase*> NearbyCages;
	const float QueryRadius = FMath::Max(Radius, FPCGExValencyCageSpatialRegistry::Get(World).GetMaxProbeRadius());
	FPCGExValencyCageSpatialRegistry::Get(World).FindCagesNearPosition(
		MyLocation,
		QueryRadius,
		NearbyCages,
		const_cast<APCGExValencyCageNull*>(this) // Exclude self
	);

	// Check if any nearby cage is a pattern cage or a participating null cage
	for (const APCGExValencyCageBase* OtherCage : NearbyCages)
	{
		if (!OtherCage)
		{
			continue;
		}

		// Check distance
		const float Distance = FVector::Dist(MyLocation, OtherCage->GetActorLocation());
		const float OtherRadius = OtherCage->GetEffectiveProbeRadius();

		// Connection is possible if either cage can reach the other
		const bool bCanReachOther = (Radius > 0.0f && Distance <= Radius);
		const bool bOtherCanReachUs = (OtherRadius > 0.0f && Distance <= OtherRadius);

		if (!bCanReachOther && !bOtherCanReachUs)
		{
			continue;
		}

		// Check if it's a pattern cage
		if (OtherCage->IsPatternCage())
		{
			return true;
		}

		// Check if it's another null cage that's participating
		if (OtherCage->IsNullCage())
		{
			if (const APCGExValencyCageNull* OtherNull = Cast<APCGExValencyCageNull>(OtherCage))
			{
				if (OtherNull->IsParticipatingInPatterns())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool APCGExValencyCageNull::DetectNearbyConnections()
{
	// Check if we should participate in patterns (auto-detect based on proximity)
	const bool bWasParticipating = bIsParticipatingInPatterns;
	bIsParticipatingInPatterns = HasNearbyPatternCages();

	// If not participating, clear orbitals and act as passive marker
	if (!bIsParticipatingInPatterns)
	{
		const bool bHadOrbitals = !Orbitals.IsEmpty();
		Orbitals.Empty();

		// Return true if state changed (had orbitals before)
		return bHadOrbitals || (bWasParticipating != bIsParticipatingInPatterns);
	}

	// Participating - ensure orbitals are initialized
	if (Orbitals.IsEmpty())
	{
		InitializeOrbitalsFromSet();
	}

	// Call base implementation for actual connection detection
	const bool bConnectionsChanged = Super::DetectNearbyConnections();

	// Return true if connections changed or participation state changed
	return bConnectionsChanged || (bWasParticipating != bIsParticipatingInPatterns);
}

bool APCGExValencyCageNull::ShouldConsiderCageForConnection(const APCGExValencyCageBase* CandidateCage) const
{
	if (!CandidateCage)
	{
		return false;
	}

	// Only consider connections when participating in patterns
	if (!bIsParticipatingInPatterns)
	{
		return false;
	}

	// Connect to pattern cages
	if (CandidateCage->IsPatternCage())
	{
		return true;
	}

	// Connect to other participating null cages
	if (CandidateCage->IsNullCage())
	{
		if (const APCGExValencyCageNull* OtherNull = Cast<APCGExValencyCageNull>(CandidateCage))
		{
			return OtherNull->IsParticipatingInPatterns();
		}
	}

	// Don't connect to regular cages (they connect TO us, not the other way)
	return false;
}
