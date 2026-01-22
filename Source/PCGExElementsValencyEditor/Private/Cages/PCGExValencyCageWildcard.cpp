// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageWildcard.h"

#include "Components/SphereComponent.h"
#include "Volumes/ValencyContextVolume.h"

APCGExValencyCageWildcard::APCGExValencyCageWildcard()
{
	// Create a small sphere for visualization and selection
	DebugSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("DebugSphere"));
	DebugSphereComponent->SetupAttachment(RootComponent);
	DebugSphereComponent->SetSphereRadius(15.0f);
	DebugSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugSphereComponent->SetLineThickness(2.0f);
	DebugSphereComponent->ShapeColor = FColor(200, 50, 200); // Magenta/purple for wildcard cages
	DebugSphereComponent->bHiddenInGame = true;
}

void APCGExValencyCageWildcard::PostEditMove(bool bFinished)
{
	// Let base class handle volume membership, connection updates, and rebuild triggering
	Super::PostEditMove(bFinished);
}

FString APCGExValencyCageWildcard::GetCageDisplayName() const
{
	if (!CageName.IsEmpty())
	{
		return FString::Printf(TEXT("WILDCARD: %s"), *CageName);
	}

	if (!Description.IsEmpty())
	{
		return FString::Printf(TEXT("WILDCARD (%s)"), *Description);
	}

	return TEXT("WILDCARD Cage");
}

void APCGExValencyCageWildcard::SetDebugComponentsVisible(bool bVisible)
{
	if (DebugSphereComponent)
	{
		DebugSphereComponent->SetVisibility(bVisible);
	}
}
