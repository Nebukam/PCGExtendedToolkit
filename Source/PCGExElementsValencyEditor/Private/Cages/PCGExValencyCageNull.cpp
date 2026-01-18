// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageNull.h"

#include "Components/SphereComponent.h"

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
