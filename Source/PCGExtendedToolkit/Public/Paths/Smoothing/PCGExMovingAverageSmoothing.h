// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSmoothingInstancedFactory.h"


#include "PCGExMovingAverageSmoothing.generated.h"

class FPCGExMovingAverageSmoothing : public FPCGExSmoothingOperation
{
public:
	virtual void SmoothSingle(
		const TSharedRef<PCGExData::FPointIO>& Path, PCGExData::FConstPoint& Target,
		const double Smoothing, const double Influence,
		const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender,
		const bool bClosedLoop) override
	{
		const int32 NumPoints = Path->GetNum();
		const int32 MaxIndex = NumPoints - 1;
		const int32 SmoothingInt = Smoothing;
		if (SmoothingInt == 0 || Influence == 0) { return; }

		const double SafeWindowSize = FMath::Max(1, SmoothingInt);
		double TotalWeight = 0;
		int32 Count = 0;
		Blender->PrepareForBlending(Target);

		if (bClosedLoop)
		{
			for (int i = -SafeWindowSize; i <= SafeWindowSize; i++)
			{
				const int32 Index = PCGExMath::Tile(Target.Index + i, 0, MaxIndex);
				const double Weight = (1 - (static_cast<double>(FMath::Abs(i)) / SafeWindowSize)) * Influence;
				Blender->Blend(Target, Path->GetInPoint(Index), Target, Weight);
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
				Blender->Blend(Target, Path->GetInPointRef(Index), Target, Weight);
				Count++;
				TotalWeight += Weight;
			}
		}

		if (Count == 0) { return; }

		Blender->CompleteBlending(Target, Count, TotalWeight);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, DisplayName = "Moving Average")
class UPCGExMovingAverageSmoothing : public UPCGExSmoothingInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSmoothingOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(MovingAverageSmoothing)
		return NewOperation;
	}
};
