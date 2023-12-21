// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExPropertiesBlender.h"

void UPCGExMovingAverageSmoothing::InternalDoSmooth(
	PCGExData::FPointIO& InPointIO)
{
	const double SafeWindowSize = FMath::Max(2, WindowSize);

	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = InPointIO.GetOut()->GetMutablePoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(&BlendingSettings);

	MetadataBlender->PrepareForData(InPointIO);

	const int32 MaxPointIndex = InPoints.Num() - 1;

	for (int i = 0; i <= MaxPointIndex; i++)
	{
		FPCGPoint& OutPoint = OutPoints[i];

		int32 Count = 0;
		MetadataBlender->PrepareForBlending(i);

		for (int j = -SafeWindowSize; j <= SafeWindowSize; j++)
		{
			const int32 Index = FMath::Clamp(i + j, 0, MaxPointIndex);
			const double Alpha = 1 - (static_cast<double>(FMath::Abs(j)) / SafeWindowSize);

			MetadataBlender->Blend(i, Index, i, Alpha);

			Count++;
		}

		MetadataBlender->CompleteBlending(i, Count);
	}

	MetadataBlender->Write();

	PCGEX_DELETE(MetadataBlender)
}
