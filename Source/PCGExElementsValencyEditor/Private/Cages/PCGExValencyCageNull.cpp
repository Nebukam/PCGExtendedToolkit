// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageNull.h"

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
	DebugSphereComponent->ShapeColor = FColor(255, 100, 100); // Reddish color for null cages
	DebugSphereComponent->bHiddenInGame = true;
}

void APCGExValencyCageNull::PostEditMove(bool bFinished)
{
	// Let base class handle volume membership, connection updates, etc.
	Super::PostEditMove(bFinished);

	// After drag finishes, trigger auto-rebuild for containing volumes
	// Null cages represent boundaries, so moving them affects which regular cages
	// have boundary connections vs regular connections
	if (bFinished && AValencyContextVolume::IsValencyModeActive())
	{
		TriggerAutoRebuildIfNeeded();
	}
}

FString APCGExValencyCageNull::GetCageDisplayName() const
{
	if (!CageName.IsEmpty())
	{
		return FString::Printf(TEXT("NULL: %s"), *CageName);
	}

	if (!Description.IsEmpty())
	{
		return FString::Printf(TEXT("NULL (%s)"), *Description);
	}

	return TEXT("NULL Cage");
}

void APCGExValencyCageNull::SetDebugComponentsVisible(bool bVisible)
{
	if (DebugSphereComponent)
	{
		DebugSphereComponent->SetVisibility(bVisible);
	}
}
