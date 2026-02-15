// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExValencyConnectorSet.h"
#include "Growth/PCGExConnectorConstraintResolver.h"
#include "Math/PCGExMathAxis.h"

#include "PCGExConstraint_AngularRange.generated.h"

/**
 * Generator constraint: rotates child module around a selectable connector axis.
 * Produces N evenly-spaced rotation steps within a specified angular range
 * defined by a center angle and half-width.
 */
USTRUCT(BlueprintType, DisplayName="Angular Range")
struct PCGEXELEMENTSVALENCY_API FPCGExConstraint_AngularRange : public FPCGExConnectorConstraint
{
	GENERATED_BODY()

	/** Axis around which to rotate variants, in connector-local space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	EPCGExAxis RotationAxis = EPCGExAxis::Forward;

	/** Center of the angular range in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = -360, ClampMax = 360))
	float CenterAngleDegrees = 0.0f;

	/** Half-width of the angular range in degrees (total sweep = 2 * HalfWidth) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 0, ClampMax = 360))
	float HalfWidthDegrees = 180.0f;

	/** Number of evenly-spaced rotation steps within the range */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint", meta = (ClampMin = 1))
	int32 Steps = 4;

	/** Add a random angular offset to each step for natural variation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Constraint")
	bool bRandomOffset = false;

	float GetMinAngle() const { return CenterAngleDegrees - HalfWidthDegrees; }
	float GetMaxAngle() const { return CenterAngleDegrees + HalfWidthDegrees; }

	virtual EPCGExConstraintRole GetRole() const override { return EPCGExConstraintRole::Generator; }
	virtual int32 GetMaxVariants() const override { return Steps; }

	virtual void GenerateVariants(
		const FPCGExConstraintContext& Context,
		FRandomStream& Random,
		TArray<FTransform>& OutVariants) const override
	{
		const FVector AxisDir = PCGExMath::GetDirection(Context.ParentConnectorWorld.GetRotation(), RotationAxis);
		const FVector RotationCenter = Context.ParentConnectorWorld.GetTranslation();

		const float MinAngle = GetMinAngle();
		const float MaxAngle = GetMaxAngle();
		const float Range = MaxAngle - MinAngle;
		const float StepSize = (Steps > 1) ? (Range / static_cast<float>(Steps)) : 0.0f;

		for (int32 i = 0; i < Steps; ++i)
		{
			float Angle = MinAngle + StepSize * static_cast<float>(i);

			if (bRandomOffset)
			{
				Angle += Random.FRand() * StepSize;
			}

			const FQuat StepRotation(AxisDir, FMath::DegreesToRadians(Angle));

			FTransform Variant = Context.BaseAttachment;
			const FVector Offset = Variant.GetTranslation() - RotationCenter;
			const FVector RotatedOffset = StepRotation.RotateVector(Offset);
			Variant.SetTranslation(RotationCenter + RotatedOffset);
			Variant.SetRotation(StepRotation * Variant.GetRotation());

			OutVariants.Add(Variant);
		}
	}
};
