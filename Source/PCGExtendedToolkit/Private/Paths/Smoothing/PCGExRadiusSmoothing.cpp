// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExRadiusSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

void UPCGExRadiusSmoothing::InternalDoSmooth(
	PCGExData::FPointIO& InPointIO)
{
	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = InPointIO.GetOut()->GetMutablePoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(&BlendingSettings);
	MetadataBlender->PrepareForData(InPointIO);

	const double RadiusSquared = BlendRadius * BlendRadius;
	const int32 MaxPointIndex = InPoints.Num() - 1;
	for (int i = 0; i <= MaxPointIndex; i++)
	{
		FVector Origin = InPoints[i].Transform.GetLocation();
		FPCGPoint& OutPoint = OutPoints[i];
		int32 Count = 0;

		MetadataBlender->PrepareForBlending(i);

		for (int j = 0; j <= MaxPointIndex; j++)
		{
			const FPCGPoint& InPoint = InPoints[j];
			const double Dist = FVector::DistSquared(Origin, InPoint.Transform.GetLocation());
			if (Dist <= RadiusSquared)
			{
				const double Alpha = 1 - (Dist / RadiusSquared);
				MetadataBlender->Blend(i, j, i, Alpha);
				Count++;
			}
		}

		MetadataBlender->CompleteBlending(i, Count);
	}

	MetadataBlender->Write();

	PCGEX_DELETE(MetadataBlender)
}
