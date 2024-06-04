// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExMovingAverageSmoothing::DoSmooth(PCGExData::FPointIO& InPointIO, const TArray<double>* Smoothing, const TArray<double>* Influence, const bool bClosedPath, const FPCGExBlendingSettings* BlendingSettings)
{
	const TArray<FPCGPoint>& InPoints = InPointIO.GetIn()->GetPoints();

	PCGExDataBlending::FMetadataBlender* MetadataBlender = new PCGExDataBlending::FMetadataBlender(const_cast<FPCGExBlendingSettings*>(BlendingSettings));

	MetadataBlender->PrepareForData(InPointIO);

	const int32 MaxPointIndex = InPoints.Num() - 1;

	if (bClosedPath)
	{
		for (int i = 0; i <= MaxPointIndex; i++)
		{
			const int32 SmoothingAmount = (*Smoothing)[i];
			const double InfluenceAmount = (*Influence)[i];

			if (SmoothingAmount == 0 || InfluenceAmount == 0) { continue; }

			const double SafeWindowSize = FMath::Max(2, SmoothingAmount);

			int32 Count = 0;
			PCGEx::FPointRef Target = InPointIO.GetOutPointRef(i);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int j = -SafeWindowSize; j <= SafeWindowSize; j++)
			{
				const int32 Index = PCGExMath::Tile(i + j, 0, MaxPointIndex);
				const double Weight = (1 - (static_cast<double>(FMath::Abs(j)) / SafeWindowSize)) * InfluenceAmount;
				MetadataBlender->Blend(Target, InPointIO.GetInPointRef(Index), Target, Weight);
				Count++;
				TotalWeight += Weight;
			}

			MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
		}
	}
	else
	{
		for (int i = 0; i <= MaxPointIndex; i++)
		{
			const int32 SmoothingAmount = (*Smoothing)[i];
			const double InfluenceAmount = (*Influence)[i];

			if (SmoothingAmount == 0 || InfluenceAmount == 0) { continue; }

			const double SafeWindowSize = FMath::Max(2, SmoothingAmount);

			int32 Count = 0;
			PCGEx::FPointRef Target = InPointIO.GetOutPointRef(i);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int j = -SafeWindowSize; j <= SafeWindowSize; j++)
			{
				const int32 Index = i + j;
				if (!InPoints.IsValidIndex(Index)) { continue; }

				const double Weight = (1 - (static_cast<double>(FMath::Abs(j)) / SafeWindowSize)) * InfluenceAmount;
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
