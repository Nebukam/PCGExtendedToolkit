// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Data/Blending/PCGExMetadataBlender.h"

void UPCGExMovingAverageSmoothing::SmoothSingle(
	PCGExData::FPointIO* Path,
	PCGExData::FPointRef& Target,
	const double Smoothing,
	const double Influence,
	PCGExDataBlending::FMetadataBlender* MetadataBlender,
	const bool bClosedPath)
{
	const int32 NumPoints = Path->GetNum();
	const int32 SmoothingInt = Smoothing;
	if (SmoothingInt == 0 || Influence == 0) { return; }

	const double SafeWindowSize = FMath::Max(2, SmoothingInt);
	double TotalWeight = 0;
	int32 Count = 0;
	MetadataBlender->PrepareForBlending(Target);

	if (bClosedPath)
	{
		for (int i = -SafeWindowSize; i <= SafeWindowSize; i++)
		{
			const int32 Index = PCGExMath::Tile(Target.Index + i, 0, NumPoints);
			const double Weight = (1 - (static_cast<double>(FMath::Abs(i)) / SafeWindowSize)) * Influence;
			MetadataBlender->Blend(Target, Path->GetInPointRef(Index), Target, Weight);
			Count++;
			TotalWeight += Weight;
		}
	}
	else
	{
		for (int i = -SafeWindowSize; i <= SafeWindowSize; i++)
		{
			const int32 Index = Target.Index + i;
			if (!FMath::IsWithin(Index, 0, NumPoints)) { continue; }

			const double Weight = (1 - (static_cast<double>(FMath::Abs(i)) / SafeWindowSize)) * Influence;
			MetadataBlender->Blend(Target, Path->GetInPointRef(Index), Target, Weight);
			Count++;
			TotalWeight += Weight;
		}
	}

	MetadataBlender->CompleteBlending(Target, Count, TotalWeight);
}
