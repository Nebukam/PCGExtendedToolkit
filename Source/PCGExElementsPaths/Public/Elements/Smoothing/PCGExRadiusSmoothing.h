// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSmoothingInstancedFactory.h"
#include "Core/PCGExProxyDataBlending.h"
#include "Data/PCGExPointIO.h"
#include "Factories/PCGExFactoryData.h"

#include "PCGExRadiusSmoothing.generated.h"

class FPCGExRadiusSmoothing : public FPCGExSmoothingOperation
{
public:
	virtual void SmoothSingle(const int32 TargetIndex, const double Smoothing, const double Influence, TArray<PCGEx::FOpStats>& Trackers) override
	{
		const double RadiusSquared = Smoothing * Smoothing;

		if (Influence == 0) { return; }

		TConstPCGValueRange<FTransform> InTransforms = Path->GetIn()->GetConstTransformValueRange();

		const FVector Origin = InTransforms[TargetIndex].GetLocation();

		Blender->BeginMultiBlend(TargetIndex, Trackers);

		Path->GetIn()->GetPointOctree().FindElementsWithBoundsTest(FBoxCenterAndExtent(Origin, FVector(Smoothing)), [&](const PCGPointOctree::FPointRef& PointRef)
		{
			const double Dist = FVector::DistSquared(Origin, InTransforms[PointRef.Index].GetLocation());
			if (Dist >= RadiusSquared || PointRef.Index == TargetIndex) { return; }

			Blender->MultiBlend(PointRef.Index, TargetIndex, (1 - (Dist / RadiusSquared)) * Influence, Trackers);
		});

		Blender->EndMultiBlend(TargetIndex, Trackers);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Radius", PCGExNodeLibraryDoc="paths/smooth/smooth-radius"))
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
