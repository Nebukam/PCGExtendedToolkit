// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGeo.h"
#include "PCGExGeoDelaunay.h"
#include "PCGExGeoVoronoi.h"
#include "PCGExMT.h"

namespace PCGExGeoTask
{
	class PCGEXTENDEDTOOLKIT_API FLloydRelax2 : public FPCGExNonAbandonableTask
	{
	public:
		FLloydRelax2(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
			TArray<FVector2D>* InPositions, const FPCGExInfluenceSettings& InInfluenceSettings, const int32 InNumIterations, PCGEx::FLocalSingleFieldGetter* InInfluenceGetter = nullptr) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			ActivePositions(InPositions), InfluenceSettings(InInfluenceSettings), NumIterations(InNumIterations), InfluenceGetter(InInfluenceGetter)
		{
		}

		TArray<FVector2D>* ActivePositions = nullptr;
		FPCGExInfluenceSettings InfluenceSettings;
		int32 NumIterations = 0;
		PCGEx::FLocalSingleFieldGetter* InfluenceGetter = nullptr;

		virtual bool ExecuteTask() override
		{
			NumIterations--;

			PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();
			TArray<FVector2D>& Positions = *ActivePositions;

			const TArrayView<FVector2D> View = MakeArrayView(Positions);
			if (!Delaunay->Process(View)) { return false; }

			const int32 NumPoints = Positions.Num();

			TArray<FVector2D> Sum;
			TArray<double> Counts;
			Sum.Append(*ActivePositions);
			Counts.SetNum(NumPoints);
			for (int i = 0; i < NumPoints; i++) { Counts[i] = 1; }

			FVector2D Centroid;
			for (const PCGExGeo::FDelaunaySite2& Site : Delaunay->Sites)
			{
				PCGExGeo::GetCentroid(Positions, Site.Vtx, Centroid);
				for (const int32 PtIndex : Site.Vtx)
				{
					Counts[PtIndex] += 1;
					Sum[PtIndex] += Centroid;
				}
			}

			if (InfluenceSettings.bProgressiveInfluence && InfluenceGetter)
			{
				for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceGetter->SafeGet(i, InfluenceSettings.Influence)); }
			}
			else
			{
				for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceSettings.Influence); }
			}

			PCGEX_DELETE(Delaunay)

			if (NumIterations > 0)
			{
				Manager->Start<FLloydRelax2>(TaskIndex + 1, PointIO, ActivePositions, InfluenceSettings, NumIterations);
			}

			return true;
		}
	};

	class PCGEXTENDEDTOOLKIT_API FLloydRelax3 : public FPCGExNonAbandonableTask
	{
	public:
		FLloydRelax3(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
			TArray<FVector>* InPositions, const FPCGExInfluenceSettings& InInfluenceSettings, const int32 InNumIterations, PCGEx::FLocalSingleFieldGetter* InInfluenceGetter = nullptr) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			ActivePositions(InPositions), InfluenceSettings(InInfluenceSettings), NumIterations(InNumIterations), InfluenceGetter(InInfluenceGetter)
		{
		}

		TArray<FVector>* ActivePositions = nullptr;
		FPCGExInfluenceSettings InfluenceSettings;
		int32 NumIterations = 0;
		PCGEx::FLocalSingleFieldGetter* InfluenceGetter = nullptr;

		virtual bool ExecuteTask() override
		{
			NumIterations--;

			PCGExGeo::TDelaunay3* Delaunay = new PCGExGeo::TDelaunay3();
			TArray<FVector>& Positions = *ActivePositions;

			const TArrayView<FVector> View = MakeArrayView(Positions);
			if (!Delaunay->Process(View)) { return false; }

			const int32 NumPoints = Positions.Num();

			TArray<FVector> Sum;
			TArray<double> Counts;
			Sum.Append(*ActivePositions);
			Counts.SetNum(NumPoints);
			for (int i = 0; i < NumPoints; i++) { Counts[i] = 1; }

			FVector Centroid;
			for (const PCGExGeo::FDelaunaySite3& Site : Delaunay->Sites)
			{
				PCGExGeo::GetCentroid(Positions, Site.Vtx, Centroid);
				for (const int32 PtIndex : Site.Vtx)
				{
					Counts[PtIndex] += 1;
					Sum[PtIndex] += Centroid;
				}
			}

			if (InfluenceSettings.bProgressiveInfluence && InfluenceGetter)
			{
				for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceGetter->SafeGet(i, InfluenceSettings.Influence)); }
			}
			else
			{
				for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceSettings.Influence); }
			}

			PCGEX_DELETE(Delaunay)

			if (NumIterations > 0)
			{
				Manager->Start<FLloydRelax3>(TaskIndex + 1, PointIO, ActivePositions, InfluenceSettings, NumIterations);
			}

			return true;
		}
	};
}
