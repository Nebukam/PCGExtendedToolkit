// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExRadiusSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"


void UPCGExRadiusSmoothing::DoSmooth(PCGExData::FPointIO& InPointIO, const TArray<double>* Smoothing, const TArray<double>* Influence, const bool bClosedPath, const FPCGExBlendingSettings* BlendingSettings)
{
	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(BlendingSettings));
	MetadataBlender->PrepareForData(InPointIO);

	const int32 MaxPointIndex = InPoints.Num() - 1;
	for (int i = 0; i <= MaxPointIndex; i++)
	{
		const double RadiusSquared = (*Smoothing)[i] * (*Smoothing)[i];
		const double SmoothingInfluence = (*Influence)[i];

		if (SmoothingInfluence == 0) { continue; }

		FVector Origin = InPoints[i].Transform.GetLocation();
		int32 Count = 0;

		PCGEx::FPointRef Target = InPointIO.GetOutPointRef(i);
		MetadataBlender->PrepareForBlending(Target);

		double TotalWeight = 0;
		for (int j = 0; j <= MaxPointIndex; j++)
		{
			const FPCGPoint& InPoint = InPoints[j];
			const double Dist = FVector::DistSquared(Origin, InPoint.Transform.GetLocation());
			if (Dist <= RadiusSquared)
			{
				const double Weight = (1 - (Dist / RadiusSquared)) * SmoothingInfluence;
				MetadataBlender->Blend(Target, InPointIO.GetInPointRef(j), Target, Weight);
				Count++;
				TotalWeight += Weight;
			}
		}

		MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
	}

	MetadataBlender->Write();

	PCGEX_DELETE(MetadataBlender)
}
