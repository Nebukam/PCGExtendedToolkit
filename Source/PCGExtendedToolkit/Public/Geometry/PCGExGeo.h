// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"

UENUM(BlueprintType)
enum class EPCGExDelaunay2DNormal : uint8
{
	Static UMETA(DisplayName = "Static Normal", ToolTip="Static normal"),
	Local UMETA(DisplayName = "Local Normal", ToolTip="Local normal fetched from point attribute"),
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
