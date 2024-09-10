// Copyright Timothé Lapetite 2024
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRadiusSmoothing : public UPCGExSmoothingOperation
{
	GENERATED_BODY()

public:
	virtual void SmoothSingle(
		PCGExData::FPointIO* Path,
		PCGExData::FPointRef& Target,
		const double Smoothing,
		const double Influence,
		PCGExDataBlending::FMetadataBlender* MetadataBlender,
		const bool bClosedPath) override
	{
		const double RadiusSquared = Smoothing * Smoothing;

		if (Influence == 0) { return; }

		const FVector Origin = Target.Point->Transform.GetLocation();
		int32 Count = 0;

		MetadataBlender->PrepareForBlending(Target);

		const FPCGPoint* StartData = Path->GetIn()->GetPoints().GetData();
		
		double TotalWeight = 0;
		Path->GetIn()->GetOctree().FindElementsWithBoundsTest(
			FBoxCenterAndExtent(Origin, FVector(Smoothing)), [&](const FPCGPointRef& Ref)
			{
				const double Dist = FVector::DistSquared(Origin, Ref.Point->Transform.GetLocation());
				const int32 OtherIndex = Ref.Point - StartData;
				if (Dist >= RadiusSquared || OtherIndex == Target.Index) { return; }

				const double Weight = (1 - (Dist / RadiusSquared)) * Influence;
				
				MetadataBlender->Blend(Target, Path->GetInPointRef(OtherIndex), Target, Weight);
				Count++;
				TotalWeight += Weight;
			});

		if (Count == 0) { return; }

		MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
	}
};
