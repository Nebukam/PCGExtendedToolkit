// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Growth/PCGExConnectorConstraintResolver.h"

#include "PCGExConstraint_VolumeOffset.generated.h"

/**
 * Modifier constraint: applies a random offset within a 3D box around the connector point.
 * The box is oriented in the parent connector's local space.
 */
USTRUCT(BlueprintType, DisplayName="Volume Offset")
struct PCGEXELEMENTSVALENCY_API FPCGExConstraint_VolumeOffset : public FPCGExConnectorConstraint
{
	GENERATED_BODY()

	/** Half-extent of the offset box in each axis (connector-local space) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0))
	FVector Extent = FVector(10.0f);

	virtual EPCGExConstraintRole GetRole() const override { return EPCGExConstraintRole::Modifier; }

	virtual void ApplyModification(
		const FPCGExConstraintContext& Context,
		FTransform& InOutTransform,
		FRandomStream& Random) const override
	{
		// Get connector-local axes
		const FQuat ConnectorRot = Context.ParentConnectorWorld.GetRotation();
		const FVector Forward = ConnectorRot.GetForwardVector();
		const FVector Right = ConnectorRot.GetRightVector();
		const FVector Up = ConnectorRot.GetUpVector();

		// Random offset within [-Extent, Extent] per axis
		const float OffsetFwd = (Random.FRand() - 0.5f) * 2.0f * Extent.X;
		const float OffsetRight = (Random.FRand() - 0.5f) * 2.0f * Extent.Y;
		const float OffsetUp = (Random.FRand() - 0.5f) * 2.0f * Extent.Z;

		const FVector Offset = Forward * OffsetFwd + Right * OffsetRight + Up * OffsetUp;
		InOutTransform.AddToTranslation(Offset);
	}
};
