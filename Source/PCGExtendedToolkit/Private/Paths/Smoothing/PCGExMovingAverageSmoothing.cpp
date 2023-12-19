// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

void UPCGExMovingAverageSmoothing::InternalDoSmooth(
	const PCGEx::FLocalSingleFieldGetter& InfluenceGetter,
	PCGExDataBlending::FMetadataBlender* MetadataInfluence,
	PCGExDataBlending::FPropertiesBlender* PropertiesInfluence,
	PCGExData::FPointIO& InPointIO)
{
	const double SafeWindowSize = FMath::Max(2, WindowSize);

	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = InPointIO.GetOut()->GetMutablePoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(BlendingSettings.DefaultBlending);
	PCGExDataBlending::FPropertiesBlender* PropertiesBlender = new PCGExDataBlending::FPropertiesBlender(BlendingSettings);

	MetadataBlender->PrepareForData(&InPointIO, BlendingSettings.AttributesOverrides);

	const int32 MaxPointIndex = InPoints.Num() - 1;

	for (int i = 0; i <= MaxPointIndex; i++)
	{
		const double ReverseSmooth = GetReverseInfluence(InfluenceGetter, i);
		if (ReverseSmooth == 1) { continue; }

		FPCGPoint& OutPoint = OutPoints[i];

		int32 Count = 0;
		MetadataBlender->PrepareForBlending(i);
		PropertiesBlender->PrepareBlending(OutPoint, OutPoint);

		for (int j = -SafeWindowSize; j <= SafeWindowSize; j++)
		{
			const int32 Index = FMath::Clamp(i + j, 0, MaxPointIndex);
			const double Alpha = 1 - (static_cast<double>(FMath::Abs(j)) / SafeWindowSize);

			MetadataBlender->Blend(i, Index, i, Alpha);
			PropertiesBlender->Blend(OutPoint, InPoints[Index], OutPoint, Alpha);

			Count++;
		}

		MetadataBlender->CompleteBlending(i, Count);
		PropertiesBlender->CompleteBlending(OutPoint);

		MetadataInfluence->Blend(i, i, i, ReverseSmooth);
		PropertiesInfluence->Blend(OutPoint, InPoints[i], OutPoint, ReverseSmooth);
	}

	PCGEX_DELETE(MetadataBlender)
	PCGEX_DELETE(PropertiesBlender)
}
