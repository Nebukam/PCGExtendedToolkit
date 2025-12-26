// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCTypes.h"
#include "PCGExCCPolyline.h"
#include "PCGExCCBoolean.generated.h"

/**
 * Options for boolean operations
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSCAVALIERCONTOURS_API FContourBooleanOptions
{
	GENERATED_BODY()

	/** Epsilon for position equality tests */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boolean", AdvancedDisplay)
	double PositionEqualEpsilon = 1e-5;
};

/**
 * Result status of a boolean operation
 */
UENUM(BlueprintType)
enum class EBooleanResultInfo : uint8
{
	/** Operation completed with intersections found and processed */
	Intersected UMETA(DisplayName = "Intersected"),

	/** Polylines completely overlap */
	Overlapping UMETA(DisplayName = "Overlapping"),

	/** Polyline 1 is completely inside polyline 2 */
	Pline1InsidePline2 UMETA(DisplayName = "Pline1 Inside Pline2"),

	/** Polyline 2 is completely inside polyline 1 */
	Pline2InsidePline1 UMETA(DisplayName = "Pline2 Inside Pline1"),

	/** Polylines are disjoint (no overlap or intersection) */
	Disjoint UMETA(DisplayName = "Disjoint"),

	/** Invalid input (open polyline or insufficient vertices) */
	InvalidInput UMETA(DisplayName = "Invalid Input")
};

namespace PCGExCavalier
{
	namespace BooleanOps
	{
		/**
		 * Result of a boolean operation between two polylines
		 */
		struct PCGEXELEMENTSCAVALIERCONTOURS_API FBooleanResult
		{
			/** Positive space polylines (outer boundaries) */
			TArray<FPolyline> PositivePolylines;

			/** Negative space polylines (holes/islands) */
			TArray<FPolyline> NegativePolylines;

			/** Information about the result status */
			EBooleanResultInfo ResultInfo = EBooleanResultInfo::InvalidInput;

			/** Returns true if the result contains any polylines */
			bool HasResult() const
			{
				return PositivePolylines.Num() > 0 || NegativePolylines.Num() > 0;
			}

			/** Returns total number of resulting polylines */
			int32 TotalPolylineCount() const
			{
				return PositivePolylines.Num() + NegativePolylines.Num();
			}

			/** Returns true if the operation was successful (not invalid input) */
			bool IsValid() const
			{
				return ResultInfo != EBooleanResultInfo::InvalidInput;
			}
		};


		/**
		 * Perform boolean operation between two polylines.
		 * Both polylines should be closed.
		 * @param Pline1 The first polyline
		 * @param Pline2 The other polyline
		 * @param Operation Boolean operation type
		 * @param Options Boolean options
		 * @return Boolean result containing positive and negative space polylines
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FBooleanResult PerformBoolean(
			const FPolyline& Pline1,
			const FPolyline& Pline2,
			EPCGExCCBooleanOp Operation,
			const FContourBooleanOptions& Options);
	}
}
