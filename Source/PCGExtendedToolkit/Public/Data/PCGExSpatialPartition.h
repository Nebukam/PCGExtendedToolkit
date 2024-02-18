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
		FBox Bounds;
		TArray<uint64> Indices;

		TCluster(const FBox& InBounds):
			Bounds(InBounds)
		{
			Indices.Empty();
		}
	};

	struct PCGEXTENDEDTOOLKIT_API TMainCluster
	{
		FBox Bounds;
		uint16 Splits;
		FVector ClusterSize;

		TMap<uint64, TCluster*> Clusters;

		explicit TMainCluster(const FBox& InBounds, const uint16 InSplits = 255):
			Bounds(InBounds), Splits(InSplits)
		{
			ClusterSize = Bounds.GetSize() / static_cast<double>(Splits);
		}

		explicit TMainCluster(const UPCGPointData* InData, const uint16 InSplits = 255)
			: TMainCluster(InData->GetBounds().ExpandBy(100), InSplits)
		{
		}

		uint64 GetClusterId(const FVector& Position) const
		{
			return PCGEx::H6416(
				FMath::Floor(Position.X / ClusterSize.X),
				FMath::Floor(Position.Y / ClusterSize.Y),
				FMath::Floor(Position.Z / ClusterSize.Z), 0);
		}

		TCluster* GetOrCreateCluster(const FVector& Position)
		{
			const uint16 X = FMath::Floor(Position.X / ClusterSize.X);
			const uint16 Y = FMath::Floor(Position.Y / ClusterSize.Y);
			const uint16 Z = FMath::Floor(Position.Z / ClusterSize.Z);

			const uint64 ClusterId = PCGEx::H6416(X, Y, Z, 0);

			TCluster* Cluster;
			TCluster** ClusterPtr = Clusters.Find(ClusterId);

			if (!ClusterPtr)
			{
				const FVector LocalMin = FVector(X * ClusterSize.X, Y * ClusterSize.Y, Z * ClusterSize.Z);
				Cluster = new TCluster(FBox(Bounds.Min + LocalMin, Bounds.Min + LocalMin + ClusterSize));
				Clusters.Add(ClusterId, Cluster);
			}
			else { Cluster = *ClusterPtr; }

			return Cluster;
		}

		void Insert(const int32 IOIndex, const UPCGPointData* InData)
		{
			const TArray<FPCGPoint>& InPoints = InData->GetPoints();
			const int32 NumPoints = InPoints.Num();
			for (int i = 0; i < NumPoints; i++) { Insert(InPoints[i].Transform.GetLocation(), PCGEx::H64(IOIndex, i)); }
		}

		void Insert(const FVector& Position, const uint64 Value)
		{
			TCluster* Cluster = GetOrCreateCluster(Position - Bounds.Min);
			Cluster->Indices.Add(Value);
		}

		template <typename IterateBoundsFunc>
		void IterateWithinBounds(const FBoxCenterAndExtent& BoxBounds, const IterateBoundsFunc& Func)
		{
			const FBox Box = BoxBounds.GetBox();
			const uint64 From = GetClusterId(Box.Min - Bounds.Min);
			const uint64 To = GetClusterId(Box.Max - Bounds.Min);

			uint16 Dummy;

			uint16 FromX;
			uint16 FromY;
			uint16 FromZ;

			uint16 ToX;
			uint16 ToY;
			uint16 ToZ;

			PCGEx::H6416(From, FromX, FromY, FromZ, Dummy);
			PCGEx::H6416(To, ToX, ToY, ToZ, Dummy);

			for (int x = FromX; x < ToX; x++)
			{
				for (int y = FromY; y < ToY; y++)
				{
					for (int z = FromZ; z < ToZ; z++)
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
