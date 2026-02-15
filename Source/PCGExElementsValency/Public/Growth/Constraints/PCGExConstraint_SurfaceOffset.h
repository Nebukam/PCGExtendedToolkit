// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Growth/PCGExConnectorConstraintResolver.h"

#include "PCGExConstraint_SurfaceOffset.generated.h"

/**
 * Modifier constraint: slides the attachment point within a rectangular region
 * on the connector face plane.
 */
USTRUCT(BlueprintType, DisplayName="Surface Offset")
struct PCGEXELEMENTSVALENCY_API FPCGExConstraint_SurfaceOffset : public FPCGExConnectorConstraint
{
	GENERATED_BODY()

	/** Width of the offset rectangle (along connector's right axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0))
	float Width = 10.0f;

	/** Height of the offset rectangle (along connector's up axis) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0))
	float Height = 10.0f;

	virtual EPCGExConstraintRole GetRole() const override { return EPCGExConstraintRole::Modifier; }

	virtual void ApplyModification(
		const FPCGExConstraintContext& Context,
		FTransform& InOutTransform,
		FRandomStream& Random) const override
	{
		// Get the connector face axes from the parent connector's world transform
		const FQuat ConnectorRot = Context.ParentConnectorWorld.GetRotation();
		const FVector Right = ConnectorRot.GetRightVector();
		const FVector Up = ConnectorRot.GetUpVector();

		// Random offset within [-Width/2, Width/2] x [-Height/2, Height/2]
		const float OffsetX = (Random.FRand() - 0.5f) * Width;
		const float OffsetY = (Random.FRand() - 0.5f) * Height;

		const FVector Offset = Right * OffsetX + Up * OffsetY;
		InOutTransform.AddToTranslation(Offset);
	}
};
