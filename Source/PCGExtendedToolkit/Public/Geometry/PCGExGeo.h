// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeoPrimtives.h"
#include "Data/PCGExPointIO.h"

namespace PCGExGeo
{
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
			for (int P = 0; P < DIMENSIONS; P++){ (*Vtx)[P] = Position[P]; }
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
