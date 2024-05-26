// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExMovingAverageSmoothing::InternalDoSmooth(
	PCGExData::FPointIO& InPointIO)
{
	const double SafeWindowSize = FMath::Max(2, WindowSize);

	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(&BlendingSettings);

	MetadataBlender->PrepareForData(InPointIO);

	const int32 MaxPointIndex = InPoints.Num() - 1;

	if (bClosedPath)
	{
		for (int i = 0; i <= MaxPointIndex; i++)
		{
			int32 Count = 0;
			PCGEx::FPointRef Target = InPointIO.GetOutPointRef(i);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int j = -SafeWindowSize; j <= SafeWindowSize; j++)
			{
				const int32 Index = PCGExMath::Tile(i + j, 0, MaxPointIndex);
				const double Alpha = 1 - (static_cast<double>(FMath::Abs(j)) / SafeWindowSize);
				MetadataBlender->Blend(Target, InPointIO.GetInPointRef(Index), Target, Alpha);
				Count++;
				TotalWeight += Alpha;
			}

			MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
		}
	}
	else
	{
		for (int i = 0; i <= MaxPointIndex; i++)
		{
			int32 Count = 0;
			PCGEx::FPointRef Target = InPointIO.GetOutPointRef(i);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int j = -SafeWindowSize; j <= SafeWindowSize; j++)
			{
				const int32 Index = i + j;
				if (!InPoints.IsValidIndex(Index)) { continue; }

				const double Weight = 1 - (static_cast<double>(FMath::Abs(j)) / SafeWindowSize);
				MetadataBlender->Blend(Target, InPointIO.GetInPointRef(Index), Target, Weight);
				Count++;
				TotalWeight += Weight;
			}

			MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
		}
	}


	MetadataBlender->Write();

	PCGEX_DELETE(MetadataBlender)
}

void UPCGExMovingAverageSmoothing::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(WindowSize, FName(TEXT("Smoothing/WindowSize")), EPCGMetadataTypes::Double);
}
