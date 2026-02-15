// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Growth/PCGExConnectorConstraintResolver.h"

#include "PCGExConstraint_AngularRange.generated.h"

/**
 * Generator constraint: rotates child module around the parent connector's forward axis.
 * Produces N evenly-spaced rotation steps within a specified angular range.
 */
USTRUCT(BlueprintType, DisplayName="Angular Range")
struct PCGEXELEMENTSVALENCY_API FPCGExConstraint_AngularRange : public FPCGExConnectorConstraint
{
	GENERATED_BODY()

	/** Start of angular range in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0, ClampMax = 360))
	float MinAngleDegrees = 0.0f;

	/** End of angular range in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0, ClampMax = 360))
	float MaxAngleDegrees = 360.0f;

	/** Number of evenly-spaced rotation steps within the range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 1))
	int32 Steps = 4;

	/** Add a random angular offset to each step for natural variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	bool bRandomOffset = false;

	virtual EPCGExConstraintRole GetRole() const override { return EPCGExConstraintRole::Generator; }
	virtual int32 GetMaxVariants() const override { return Steps; }

	virtual void GenerateVariants(
		const FPCGExConstraintContext& Context,
		FRandomStream& Random,
		TArray<FTransform>& OutVariants) const override
	{
		// Rotation axis = parent connector's forward direction
		const FVector RotationAxis = Context.ParentConnectorWorld.GetRotation().GetForwardVector();
		const FVector RotationCenter = Context.ParentConnectorWorld.GetTranslation();

		const float Range = MaxAngleDegrees - MinAngleDegrees;
		const float StepSize = (Steps > 1) ? (Range / static_cast<float>(Steps)) : 0.0f;

		for (int32 i = 0; i < Steps; ++i)
		{
			float Angle = MinAngleDegrees + StepSize * static_cast<float>(i);

			if (bRandomOffset)
			{
				// Add random offset within the step size
				Angle += Random.FRand() * StepSize;
			}

			const FQuat StepRotation(RotationAxis, FMath::DegreesToRadians(Angle));

			// Apply rotation around the parent connector position
			FTransform Variant = Context.BaseAttachment;
			const FVector Offset = Variant.GetTranslation() - RotationCenter;
			const FVector RotatedOffset = StepRotation.RotateVector(Offset);
			Variant.SetTranslation(RotationCenter + RotatedOffset);
			Variant.SetRotation(StepRotation * Variant.GetRotation());

			OutVariants.Add(Variant);
		}
	}
};
