// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFactoryProvider.h"
#include "PCGExSmoothingInstancedFactory.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "PCGExRadiusSmoothing.generated.h"

class FPCGExRadiusSmoothing : public FPCGExSmoothingOperation
{
public:
	virtual void SmoothSingle(
		const TSharedRef<PCGExData::FPointIO>& Path, PCGExData::FConstPoint& Target,
		const double Smoothing, const double Influence,
		const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender,
		const bool bClosedLoop) override
	{
		const double RadiusSquared = Smoothing * Smoothing;

		if (Influence == 0) { return; }

		const FVector Origin = Target.GetLocation();
		const UPCGBasePointData* InPointData = Path->GetIn();
		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		TArray<int32> Indices;
		TArray<double> Weights;

		Indices.Reserve(10);
		Weights.Reserve(10);

		// TODO : Reuse a shared buffer based on Scope whenever possible
		// Use a TScopedArray<PCGEx::FOpStats> ?
		TArray<PCGEx::FOpStats> Tracking;
		Blender->BeginMultiBlend(Target.Index, Tracking);

		Path->GetIn()->GetPointOctree().FindElementsWithBoundsTest(
			FBoxCenterAndExtent(Origin, FVector(Smoothing)), [&](const PCGPointOctree::FPointRef& PointRef)
			{
				const double Dist = FVector::DistSquared(Origin, InTransforms[PointRef.Index].GetLocation());
				if (Dist >= RadiusSquared || PointRef.Index == Target.Index) { return; }

				Blender->MultiBlend(PointRef.Index, Target.Index, (1 - (Dist / RadiusSquared)) * Influence, Tracking);
			});

		if (Indices.IsEmpty()) { return; }
		
		Blender->EndMultiBlend(Target.Index, Tracking);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Radius")
class UPCGExRadiusSmoothing : public UPCGExSmoothingInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSmoothingOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(RadiusSmoothing)
		return NewOperation;
	}
};
