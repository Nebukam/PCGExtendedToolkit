// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExRadiusSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

void UPCGExRadiusSmoothing::InternalDoSmooth(
	const PCGEx::FLocalSingleFieldGetter& InfluenceGetter,
	PCGExDataBlending::FMetadataBlender* MetadataInfluence,
	PCGExDataBlending::FPropertiesBlender* PropertiesInfluence,
	PCGExData::FPointIO& InPointIO)
{
	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = InPointIO.GetOut()->GetMutablePoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(BlendingSettings.DefaultBlending);
	PCGExDataBlending::FPropertiesBlender* PropertiesBlender = new PCGExDataBlending::FPropertiesBlender(BlendingSettings);
	MetadataBlender->PrepareForData(&InPointIO, BlendingSettings.AttributesOverrides);

	const double RadiusSquared = BlendRadius * BlendRadius;
	const int32 MaxPointIndex = InPoints.Num() - 1;
	for (int i = 0; i <= MaxPointIndex; i++)
	{
		const double ReverseSmooth = GetReverseInfluence(InfluenceGetter, i);
		if (ReverseSmooth == 1) { continue; }

		FVector Origin = InPoints[i].Transform.GetLocation();
		FPCGPoint& OutPoint = OutPoints[i];
		int32 Count = 0;

		MetadataBlender->PrepareForBlending(i);
		PropertiesBlender->PrepareBlending(OutPoint, OutPoint);

		for (int j = 0; j <= MaxPointIndex; j++)
		{
			const FPCGPoint& InPoint = InPoints[j];
			const double Dist = FVector::DistSquared(Origin, InPoint.Transform.GetLocation());
			if (Dist <= RadiusSquared)
			{
				const double Alpha = 1 - (Dist / RadiusSquared);
				MetadataBlender->Blend(i, j, i, Alpha);
				PropertiesBlender->Blend(OutPoint, InPoint, OutPoint, Alpha);
				Count++;
			}
		}

		MetadataBlender->CompleteBlending(i, Count);
		PropertiesBlender->CompleteBlending(OutPoint);

		MetadataInfluence->Blend(i, i, i, ReverseSmooth);
		PropertiesInfluence->Blend(OutPoint, InPoints[i], OutPoint, ReverseSmooth);
	}

	PCGEX_DELETE(MetadataBlender)
	PCGEX_DELETE(PropertiesBlender)
}
