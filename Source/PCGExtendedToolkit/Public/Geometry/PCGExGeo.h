// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"
#include "PCGExGeo.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExGeo2DProjectionSettings
{
	GENERATED_BODY()

	/** Normal vector of the 2D projection plane. Defaults to Up for XY projection. Used as fallback when using invalid local normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable))
	FVector ProjectionNormal = FVector::UpVector;

	/** Uses a per-point projection normal. Use carefully as it can easily lead to singularities. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalProjectionNormal = false;

	/** Fetch the normal from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (PCG_Overridable, ShowOnlyInnerProperties, EditCondition="bUseLocalProjectionNormal"))
	FPCGExInputDescriptor LocalProjectionNormal;
};

UENUM(BlueprintType)
enum class EPCGExCellCenter : uint8
{
	Balanced UMETA(DisplayName = "Balanced", ToolTip="Pick centroid if circumcenter is out of bounds, otherwise uses circumcenter."),
	Circumcenter UMETA(DisplayName = "Canon (Circumcenter)", ToolTip="Uses Delaunay cells' circumcenter."),
	Centroid UMETA(DisplayName = "Centroid", ToolTip="Uses Delaunay cells' averaged vertice positions.")
};

namespace PCGExGeo
{
	constexpr PCGExMT::AsyncState State_ProcessingHull = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingDelaunayHull = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingDelaunayPreprocess = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingDelaunay = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingVoronoi = __COUNTER__;

	template <int DIMENSIONS>
	static void GetVerticesFromPoints(const TArray<FPCGPoint>& InPoints, TArray<TFVtx<DIMENSIONS>*>& OutVertices)
	{
		const int32 NumPoints = InPoints.Num();

		PCGEX_DELETE_TARRAY(OutVertices)
		OutVertices.SetNumUninitialized(NumPoints);

		for (int i = 0; i < NumPoints; i++)
		{
			TFVtx<DIMENSIONS>* Vtx = OutVertices[i] = new TFVtx<DIMENSIONS>();
			Vtx->Id = i;

			FVector Position = InPoints[i].Transform.GetLocation();
			Vtx->Location = Position;

			for (int P = 0; P < DIMENSIONS; P++) { (*Vtx)[P] = Position[P]; }
		}
	}

	template <int DIMENSIONS>
	static void GetUpscaledVerticesFromPoints(const TArray<FPCGPoint>& InPoints, TArray<TFVtx<DIMENSIONS>*>& OutVertices)
	{
		const int32 NumPoints = InPoints.Num();

		PCGEX_DELETE_TARRAY(OutVertices)
		OutVertices.SetNumUninitialized(NumPoints);

		for (int i = 0; i < NumPoints; i++)
		{
			TFVtx<DIMENSIONS>* Vtx = OutVertices[i] = new TFVtx<DIMENSIONS>();
			Vtx->Id = i;

			FVector Position = InPoints[i].Transform.GetLocation();
			Vtx->Location = Position;

			double SqrLn = 0;
			for (int P = 0; P < DIMENSIONS - 1; P++)
			{
				(*Vtx)[P] = Position[P];
				SqrLn += Position[P] * Position[P];
			}

			(*Vtx)[DIMENSIONS - 1] = SqrLn;
		}
	}
}
