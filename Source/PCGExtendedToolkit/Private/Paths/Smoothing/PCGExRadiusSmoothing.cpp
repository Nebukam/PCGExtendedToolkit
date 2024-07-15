// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExRadiusSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExRadiusSmoothing::SmoothSingle(
	PCGExData::FPointIO* Path,
	PCGExData::FPointRef& Target,
	const double Smoothing,
	const double Influence,
	PCGExDataBlending::FMetadataBlender* MetadataBlender,
	const bool bClosedPath)
{
	const double RadiusSquared = Smoothing * Smoothing;
	const double SmoothingInfluence = Influence;

	if (SmoothingInfluence == 0) { return; }

	const int32 MaxPointIndex = Path->GetNum() - 1;

	const FVector Origin = Target.Point->Transform.GetLocation();
	int32 Count = 0;

	MetadataBlender->PrepareForBlending(Target);

	double TotalWeight = 0;
	for (int i = 0; i <= MaxPointIndex; i++)
	{
		const FPCGPoint& InPoint = Path->GetOutPoint(i);
		const double Dist = FVector::DistSquared(Origin, InPoint.Transform.GetLocation());
		if (Dist <= RadiusSquared)
		{
			const double Weight = (1 - (Dist / RadiusSquared)) * SmoothingInfluence;
			MetadataBlender->Blend(Target, Path->GetInPointRef(i), Target, Weight);
			Count++;
			TotalWeight += Weight;
		}
	}

	MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
}
