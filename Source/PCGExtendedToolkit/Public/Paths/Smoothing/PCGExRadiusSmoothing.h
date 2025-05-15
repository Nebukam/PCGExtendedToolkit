// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSmoothingOperation.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "PCGExRadiusSmoothing.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Radius")
class UPCGExRadiusSmoothing : public UPCGExSmoothingOperation
{
	GENERATED_BODY()

public:
	virtual void SmoothSingle(
		const TSharedRef<PCGExData::FPointIO>& Path,
		PCGExData::FPointRef& Target,
		const double Smoothing,
		const double Influence,
		PCGExDataBlending::FMetadataBlender* MetadataBlender,
		const bool bClosedLoop) override
	{
		const double RadiusSquared = Smoothing * Smoothing;

		if (Influence == 0) { return; }

		const FVector Origin = Target.Point->Transform.GetLocation();
		const TArray<FPCGPoint>& PathPoints = Path->GetIn()->GetPoints();

		TArray<int32> Indices;
		TArray<double> Weights;

		Indices.Reserve(10);
		Weights.Reserve(10);

		double TotalWeight = 0;
		Path->GetIn()->PCGEX_POINT_OCTREE_GET().FindElementsWithBoundsTest(
			FBoxCenterAndExtent(Origin, FVector(Smoothing)), [&](const PCGEX_POINT_OCTREE_REF& PointRef)
			{
#if PCGEX_ENGINE_VERSION < 506
				const int32 OtherIndex = static_cast<int32>(PointRef.Point - PathPoints.GetData());
#else
				const int32 OtherIndex = PointRef.Index;
#endif
				const double Dist = FVector::DistSquared(Origin, PathPoints[OtherIndex].Transform.GetLocation());
				if (Dist >= RadiusSquared || OtherIndex == Target.Index) { return; }

				Indices.Add(OtherIndex);
				Weights.Add((1 - (Dist / RadiusSquared)) * Influence);
			});

		if (Indices.IsEmpty()) { return; }

		MetadataBlender->PrepareForBlending(Target);

		for (int i = 0; i < Indices.Num(); i++)
		{
			MetadataBlender->Blend(Target, Path->GetInPointRef(Indices[i]), Target, Weights[i]);
			TotalWeight += Weights[i];
		}

		MetadataBlender->CompleteBlending(Target, Indices.Num(), TotalWeight);
	}
};
