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

	/** Minimum area threshold for valid result polylines (filters collapsed regions) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boolean", AdvancedDisplay)
	double CollapsedAreaEpsilon = 1e-10;
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
		 * Source information for a polyline in a boolean operation.
		 * Associates a polyline with its originating root path.
		 */
		struct PCGEXELEMENTSCAVALIERCONTOURS_API FBooleanOperand
		{
			/** The polyline to use in the boolean operation */
			const FPolyline* Polyline = nullptr;

			/** The path ID to use for source tracking */
			int32 PathId = InvalidIndex;

			FBooleanOperand() = default;

			FBooleanOperand(const FPolyline* InPolyline, int32 InPathId)
				: Polyline(InPolyline)
				, PathId(InPathId)
			{
			}

			FBooleanOperand(const FPolyline& InPolyline, int32 InPathId)
				: Polyline(&InPolyline)
				, PathId(InPathId)
			{
			}

			bool IsValid() const { return Polyline != nullptr && Polyline->VertexCount() >= 2; }
		};


		/**
		 * Result of a boolean operation between two polylines.
		 * Each result polyline tracks which source paths contributed to it.
		 */
		struct PCGEXELEMENTSCAVALIERCONTOURS_API FBooleanResult
		{
			/** Positive space polylines (outer boundaries) */
			TArray<FPolyline> PositivePolylines;

			/** Negative space polylines (holes/islands) */
			TArray<FPolyline> NegativePolylines;

			/** Information about the result status */
			EBooleanResultInfo ResultInfo = EBooleanResultInfo::InvalidInput;

			/** All path IDs that contributed to this result */
			TSet<int32> AllContributingPathIds;

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

			/** Get all polylines (positive and negative) */
			TArray<const FPolyline*> GetAllPolylines() const
			{
				TArray<const FPolyline*> Result;
				for (const FPolyline& P : PositivePolylines)
				{
					Result.Add(&P);
				}
				for (const FPolyline& P : NegativePolylines)
				{
					Result.Add(&P);
				}
				return Result;
			}

			/** Collect all contributing path IDs from result polylines */
			void CollectContributingPathIds()
			{
				AllContributingPathIds.Empty();
				for (const FPolyline& P : PositivePolylines)
				{
					AllContributingPathIds.Append(P.GetContributingPathIds());
				}
				for (const FPolyline& P : NegativePolylines)
				{
					AllContributingPathIds.Append(P.GetContributingPathIds());
				}
			}
		};


		/**
		 * Perform boolean operation between two polylines with path tracking.
		 * Both polylines should be closed.
		 * 
		 * @param Operand1 First polyline with its source path ID
		 * @param Operand2 Second polyline with its source path ID
		 * @param Operation Boolean operation type
		 * @param Options Boolean options
		 * @return Boolean result with path tracking
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FBooleanResult PerformBoolean(
			const FBooleanOperand& Operand1,
			const FBooleanOperand& Operand2,
			EPCGExCCBooleanOp Operation,
			const FContourBooleanOptions& Options);

		/**
		 * Perform boolean operation between two polylines (legacy version).
		 * Both polylines should be closed.
		 * Uses polyline's PrimaryPathId for source tracking.
		 * 
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


		/**
		 * Perform boolean union of multiple polylines.
		 * 
		 * @param Operands Array of polylines with their source path IDs
		 * @param Options Boolean options
		 * @return Union result with path tracking
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FBooleanResult UnionAll(
			const TArray<FBooleanOperand>& Operands,
			const FContourBooleanOptions& Options);

		/**
		 * Perform boolean intersection of multiple polylines.
		 * 
		 * @param Operands Array of polylines with their source path IDs
		 * @param Options Boolean options
		 * @return Intersection result with path tracking
		 */
		PCGEXELEMENTSCAVALIERCONTOURS_API
		FBooleanResult IntersectAll(
			const TArray<FBooleanOperand>& Operands,
			const FContourBooleanOptions& Options);
	}
}
