// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Growth/PCGExConnectorConstraintResolver.h"

#include "PCGExConstraint_HemisphereOffset.generated.h"

/**
 * Modifier constraint: applies a random offset within a hemisphere oriented
 * along the parent connector's forward direction.
 */
USTRUCT(BlueprintType, DisplayName="Hemisphere Offset")
struct PCGEXELEMENTSVALENCY_API FPCGExConstraint_HemisphereOffset : public FPCGExConnectorConstraint
{
	GENERATED_BODY()

	/** Radius of the hemisphere */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0))
	float Radius = 10.0f;

	virtual EPCGExConstraintRole GetRole() const override { return EPCGExConstraintRole::Modifier; }

	virtual void ApplyModification(
		const FPCGExConstraintContext& Context,
		FTransform& InOutTransform,
		FRandomStream& Random) const override
	{
		// Generate uniform random point in unit hemisphere (rejection sampling)
		FVector Point;
		do
		{
			Point.X = Random.FRand() * 2.0f - 1.0f;
			Point.Y = Random.FRand() * 2.0f - 1.0f;
			Point.Z = Random.FRand(); // Only positive Z for hemisphere
		}
		while (Point.SizeSquared() > 1.0f);

		// Transform from Z-up hemisphere to connector-local space
		const FQuat ConnectorRot = Context.ParentConnectorWorld.GetRotation();
		const FVector Forward = ConnectorRot.GetForwardVector();
		const FVector Right = ConnectorRot.GetRightVector();
		const FVector Up = ConnectorRot.GetUpVector();

		// Map: X->Right, Y->Up, Z->Forward (hemisphere dome faces along connector forward)
		const FVector Offset = (Right * Point.X + Up * Point.Y + Forward * Point.Z) * Radius;
		InOutTransform.AddToTranslation(Offset);
	}
};
