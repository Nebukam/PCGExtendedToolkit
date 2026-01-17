// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageSimple.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"

APCGExValencyCageSimple::APCGExValencyCageSimple()
{
	// Root component is created by APCGExValencyCageBase
	// Debug shape will be created in OnConstruction
}

void APCGExValencyCageSimple::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Create or update debug shape on construction
	if (!DebugShapeComponent || CachedShapeType != DetectionShape)
	{
		RecreateDebugShape();
	}
	else
	{
		UpdateDebugShapeDimensions();
	}
}

void APCGExValencyCageSimple::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Shape type changed - trigger reconstruction to recreate the component
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageSimple, DetectionShape))
	{
		if (DetectionShape != CachedShapeType)
		{
			RerunConstructionScripts();
		}
	}
	// Dimensions changed - just update the existing component
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageSimple, BoxExtent) ||
			 PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageSimple, SphereRadius) ||
			 PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageSimple, CylinderRadius) ||
			 PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCageSimple, CylinderHalfHeight))
	{
		UpdateDebugShapeDimensions();
	}
}

void APCGExValencyCageSimple::RecreateDebugShape()
{
	// Destroy existing component properly
	if (DebugShapeComponent)
	{
		DebugShapeComponent->UnregisterComponent();
		DebugShapeComponent->DestroyComponent();
		DebugShapeComponent = nullptr;
	}

	CachedShapeType = DetectionShape;

	// Don't create components if we don't have a valid world yet
	if (!GetWorld())
	{
		return;
	}

	// Create appropriate component type with RF_Transient flag (not saved)
	switch (DetectionShape)
	{
	case EPCGExValencyCageShape::Box:
		{
			UBoxComponent* BoxComp = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
			BoxComp->SetBoxExtent(BoxExtent);
			DebugShapeComponent = BoxComp;
		}
		break;

	case EPCGExValencyCageShape::Sphere:
		{
			USphereComponent* SphereComp = NewObject<USphereComponent>(this, NAME_None, RF_Transient);
			SphereComp->SetSphereRadius(SphereRadius);
			DebugShapeComponent = SphereComp;
		}
		break;

	case EPCGExValencyCageShape::Cylinder:
		{
			UCapsuleComponent* CapsuleComp = NewObject<UCapsuleComponent>(this, NAME_None, RF_Transient);
			CapsuleComp->SetCapsuleRadius(CylinderRadius);
			CapsuleComp->SetCapsuleHalfHeight(CylinderHalfHeight);
			DebugShapeComponent = CapsuleComp;
		}
		break;
	}

	if (DebugShapeComponent)
	{
		DebugShapeComponent->SetupAttachment(RootComponent);
		DebugShapeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DebugShapeComponent->SetLineThickness(2.0f);
		DebugShapeComponent->ShapeColor = FColor(128, 128, 255);
		DebugShapeComponent->bHiddenInGame = true;
		DebugShapeComponent->RegisterComponent();
	}
}

void APCGExValencyCageSimple::UpdateDebugShapeDimensions()
{
	if (!DebugShapeComponent)
	{
		return;
	}

	switch (DetectionShape)
	{
	case EPCGExValencyCageShape::Box:
		if (UBoxComponent* BoxComp = Cast<UBoxComponent>(DebugShapeComponent))
		{
			BoxComp->SetBoxExtent(BoxExtent);
		}
		break;

	case EPCGExValencyCageShape::Sphere:
		if (USphereComponent* SphereComp = Cast<USphereComponent>(DebugShapeComponent))
		{
			SphereComp->SetSphereRadius(SphereRadius);
		}
		break;

	case EPCGExValencyCageShape::Cylinder:
		if (UCapsuleComponent* CapsuleComp = Cast<UCapsuleComponent>(DebugShapeComponent))
		{
			CapsuleComp->SetCapsuleRadius(CylinderRadius);
			CapsuleComp->SetCapsuleHalfHeight(CylinderHalfHeight);
		}
		break;
	}
}

bool APCGExValencyCageSimple::IsActorInside_Implementation(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	return ContainsPoint(Actor->GetActorLocation());
}

bool APCGExValencyCageSimple::ContainsPoint_Implementation(const FVector& WorldLocation) const
{
	const FVector CageLocation = GetActorLocation();
	const FVector LocalPoint = WorldLocation - CageLocation;

	// TODO: Apply inverse rotation if cage rotation should affect containment
	// For now, shapes are axis-aligned

	switch (DetectionShape)
	{
	case EPCGExValencyCageShape::Box:
		{
			return FMath::Abs(LocalPoint.X) <= BoxExtent.X &&
			       FMath::Abs(LocalPoint.Y) <= BoxExtent.Y &&
			       FMath::Abs(LocalPoint.Z) <= BoxExtent.Z;
		}

	case EPCGExValencyCageShape::Sphere:
		{
			return LocalPoint.SizeSquared() <= FMath::Square(SphereRadius);
		}

	case EPCGExValencyCageShape::Cylinder:
		{
			// Check height (Z axis)
			if (FMath::Abs(LocalPoint.Z) > CylinderHalfHeight)
			{
				return false;
			}

			// Check radial distance (XY plane)
			const float RadialDistSq = LocalPoint.X * LocalPoint.X + LocalPoint.Y * LocalPoint.Y;
			return RadialDistSq <= FMath::Square(CylinderRadius);
		}

	default:
		return false;
	}
}

FBox APCGExValencyCageSimple::GetBoundingBox() const
{
	const FVector CageLocation = GetActorLocation();

	switch (DetectionShape)
	{
	case EPCGExValencyCageShape::Box:
		return FBox(CageLocation - BoxExtent, CageLocation + BoxExtent);

	case EPCGExValencyCageShape::Sphere:
		return FBox(CageLocation - FVector(SphereRadius), CageLocation + FVector(SphereRadius));

	case EPCGExValencyCageShape::Cylinder:
		{
			const FVector Extent(CylinderRadius, CylinderRadius, CylinderHalfHeight);
			return FBox(CageLocation - Extent, CageLocation + Extent);
		}

	default:
		return FBox(CageLocation, CageLocation);
	}
}
