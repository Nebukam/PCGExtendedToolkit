// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMath.h"
#include "Data/PCGPointData.h"

namespace PCGExSpacePartition
{
	struct TMainCluster;

	struct PCGEXTENDEDTOOLKIT_API TCluster
	{
		TArray<int32> Indices;

		TCluster()
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API TMainCluster
	{
		uint16 Splits;
		FBox Bounds;
		FVector ClusterSize;

		TMap<uint64, TCluster*> Clusters;

		TMainCluster(const UPCGPointData* InData, const uint16 InSplits)
		{
			Splits = InSplits;
			Bounds = InData->GetBounds();
			ClusterSize = Bounds.GetSize() / static_cast<double>(Splits);
			Clusters.Reserve(Splits * Splits * Splits);

			const TArray<FPCGPoint>& InPoints = InData->GetPoints();
			const int32 NumPoints = InPoints.Num();
			for (int i = 0; i < NumPoints; i++)
			{
				//TODO: Find which x/y/z indice the point belongs to.
				//If the partition doesn't exists, create it.
			}
			/*
			for (int x = 0; x < Splits; x++)
			{
				for (int y = 0; y < Splits; y++)
				{
					for (int z = 0; z < Splits; z++)
					{
						uint64 ClusterId = PCGEx::H6416(x, y, z, 0);
						TCluster* NewCluster = new TCluster();
						Clusters.Add(ClusterId, NewCluster);
					}
				}
			}
			*/
		}

		template <typename IterateBoundsFunc>
		void IterateWithinBounds(const FBoxCenterAndExtent& BoxBounds, const IterateBoundsFunc& Func)
		{
			FBox Box = BoxBounds.GetBox();

			Box.Min = PCGExMath::Max(Box.Min, Bounds.Min);
			Box.Max = PCGExMath::Min(Box.Max, Bounds.Max);

			const FVector Size = Box.GetSize();

			const int32 IX = FMath::Max(FMath::Floor(Size.X / ClusterSize.X), 1);
			const int32 IY = FMath::Max(FMath::Floor(Size.Y / ClusterSize.Y), 1);
			const int32 IZ = FMath::Max(FMath::Floor(Size.Z / ClusterSize.Z), 1);

			FVector Snapped = FMath::GridSnap(Bounds.Min - Box.Min, ClusterSize);

			const int32 StartX = FMath::Max(FMath::Floor(Snapped.X / ClusterSize.X), Splits - 1);
			const int32 StartY = FMath::Max(FMath::Floor(Snapped.Y / ClusterSize.Y), Splits - 1);
			const int32 StartZ = FMath::Max(FMath::Floor(Snapped.Z / ClusterSize.Z), Splits - 1);

			for (int x = StartX; x < IX; x++)
			{
				for (int y = StartY; y < IY; y++)
				{
					for (int z = StartZ; z < IZ; z++)
					{
						const uint64 ClusterId = PCGEx::H6416(x, y, z, 0);
						if (TCluster** Cluster = Clusters.Find(ClusterId))
						{
							const int32 NumIterations = (*Cluster)->Indices.Num();
							for (int i = 0; i < NumIterations; i++) { Func((*Cluster)->Indices[i]); }
						}
					}
				}
			}
		}

		~TMainCluster()
		{
			PCGEX_DELETE_TMAP(Clusters, uint64)
		}
	};
}
