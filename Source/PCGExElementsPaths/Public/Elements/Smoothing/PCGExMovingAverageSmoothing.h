// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSmoothingInstancedFactory.h"
#include "Data/PCGExPointIO.h"
#include "Math/PCGExMath.h"
#include "Core/PCGExProxyDataBlending.h"

#include "PCGExMovingAverageSmoothing.generated.h"

class FPCGExMovingAverageSmoothing : public FPCGExSmoothingOperation
{
public:
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Ignore;

	virtual void SmoothSingle(const int32 TargetIndex, const double Smoothing, const double Influence, TArray<PCGEx::FOpStats>& Trackers) override
	{
		const int32 NumPoints = Path->GetNum();
		const int32 MaxIndex = NumPoints - 1;
		const int32 SmoothingInt = Smoothing;

		if (SmoothingInt == 0 || Influence == 0) { return; }

		const double SafeWindowSize = FMath::Max(1, SmoothingInt);

		Blender->BeginMultiBlend(TargetIndex, Trackers);

		if (bClosedLoop)
		{
			for (int i = -SafeWindowSize; i <= SafeWindowSize; i++)
			{
				const int32 Index = PCGExMath::Tile(TargetIndex + i, 0, MaxIndex);
				const double Weight = (1 - (static_cast<double>(FMath::Abs(i)) / SafeWindowSize)) * Influence;
				Blender->MultiBlend(Index, TargetIndex, Weight, Trackers);
			}
		}
		else
		{
			for (int i = -SafeWindowSize; i <= SafeWindowSize; i++)
			{
				const int32 Index = PCGExMath::SanitizeIndex(TargetIndex + i, MaxIndex, IndexSafety);

				if (!FMath::IsWithin(Index, 0, NumPoints)) { continue; }

				const double Weight = (1 - (static_cast<double>(FMath::Abs(i)) / SafeWindowSize)) * Influence;
				Blender->MultiBlend(Index, TargetIndex, Weight, Trackers);
			}
		}

		Blender->EndMultiBlend(TargetIndex, Trackers);
	}
};

/**
 * 
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Moving Average", PCGExNodeLibraryDoc="paths/smooth/smooth-moving-average"))
class UPCGExMovingAverageSmoothing : public UPCGExSmoothingInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExSmoothingOperation> CreateOperation() const override
	{
		PCGEX_FACTORY_NEW_OPERATION(MovingAverageSmoothing)
		NewOperation->IndexSafety = IndexSafety;
		return NewOperation;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Ignore;
};
