// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExCCTypes.h"
#include "PCGExCCDetails.generated.h"

/**
 * Settings for arc tessellation (converting arcs to line segments)
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSCAVALIERCONTOURS_API FPCGExCCArcTessellationSettings
{
	GENERATED_BODY()

	/** Mode for tessellating arcs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation")
	EPCGExCCArcTessellationMode Mode = EPCGExCCArcTessellationMode::DistanceBased;

	/** Number of segments per arc (used when Mode is FixedCount) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (EditCondition = "Mode == EPCGExCCArcTessellationMode::FixedCount", ClampMin = "1", ClampMax = "1024", EditConditionHide))
	int32 FixedSegmentCount = 8;

	/** Target maximum distance between points on arc (used when Mode is DistanceBased) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (EditCondition = "Mode == EPCGExCCArcTessellationMode::DistanceBased", ClampMin = "0.001", EditConditionHide))
	double TargetSegmentLength = 1.0;

	/** Minimum number of segments per arc (used when Mode is DistanceBased) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (EditCondition = "Mode == EPCGExCCArcTessellationMode::DistanceBased", ClampMin = "1", EditConditionHide))
	int32 MinSegmentCount = 2;

	/** Maximum number of segments per arc (used when Mode is DistanceBased) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tessellation", meta = (EditCondition = "Mode == EPCGExCCArcTessellationMode::DistanceBased", ClampMin = "1", EditConditionHide))
	int32 MaxSegmentCount = 128;

	/** Calculate number of segments for a given arc length */
	int32 CalculateSegmentCount(double ArcLength) const
	{
		if (Mode == EPCGExCCArcTessellationMode::FixedCount)
		{
			return FixedSegmentCount;
		}

		// Distance-based calculation
		int32 Count = FMath::CeilToInt(ArcLength / TargetSegmentLength);
		return FMath::Clamp(Count, MinSegmentCount, MaxSegmentCount);
	}
};

/**
 * Options for parallel offset operations
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSCAVALIERCONTOURS_API FPCGExCCOffsetOptions
{
	GENERATED_BODY()

	/** If true, handle self-intersecting polylines (more expensive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset")
	bool bHandleSelfIntersects = true;

	/** Epsilon for position equality tests */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset", AdvancedDisplay)
	double PositionEqualEpsilon = 1e-5;

	/** Epsilon for slice joining */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset", AdvancedDisplay)
	double SliceJoinEpsilon = 1e-4;

	/** Epsilon for offset distance validation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Offset", AdvancedDisplay)
	double OffsetDistanceEpsilon = 1e-4;
};

/**
 * Options for boolean operations
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSCAVALIERCONTOURS_API FPCGExContourBooleanOptions
{
	GENERATED_BODY()

	/** Epsilon for position equality tests */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boolean", AdvancedDisplay)
	double PositionEqualEpsilon = 1e-5;

	/** Minimum area threshold for valid result polylines (filters collapsed regions) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boolean", AdvancedDisplay)
	double CollapsedAreaEpsilon = 1e-10;
};