// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExCluster.h"

#include "PCGExSimpleConvexDecomposer.generated.h"

USTRUCT(BlueprintType)
struct FPCGExConvexDecompositionDetails
{
	GENERATED_BODY()

	/** Maximum allowed "concavity" - ratio of points inside hull vs on hull. 0 = all must be on hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double MaxConcavityRatio = 0.01;

	/** Minimum nodes per cell */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 MinNodesPerCell = 4;

	/** Maximum cells to produce */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 MaxCells = 32;

	/** Maximum recursion depth */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 MaxDepth = 100;
};

namespace PCGExClusters
{
	/**
	 * Simple convex cell result
	 */
	struct PCGEXELEMENTSCLUSTERS_API FConvexCell3D
	{
		TArray<int32> NodeIndices;
		FBox Bounds = FBox(ForceInit);
		FVector Centroid = FVector::ZeroVector;

		void ComputeBounds(const FCluster* Cluster)
		{
			Bounds.Init();
			Centroid = FVector::ZeroVector;
			for (int32 Idx : NodeIndices)
			{
				FVector Pos = Cluster->GetPos(Idx);
				Bounds += Pos;
				Centroid += Pos;
			}
			if (NodeIndices.Num() > 0)
			{
				Centroid /= NodeIndices.Num();
			}
		}
	};

	struct PCGEXELEMENTSCLUSTERS_API FConvexDecomposition
	{
		TArray<FConvexCell3D> Cells;

		void Clear() { Cells.Empty(); }
	};

	/**
	 * Simple, working convex decomposition for clusters
	 */
	class PCGEXELEMENTSCLUSTERS_API FSimpleConvexDecomposer
	{
	public:
		bool Decompose(
			const FCluster* Cluster,
			FConvexDecomposition& OutResult,
			const FPCGExConvexDecompositionDetails& Settings = FPCGExConvexDecompositionDetails());

		bool DecomposeSubset(
			const FCluster* Cluster,
			const TArray<int32>& NodeIndices,
			FConvexDecomposition& OutResult,
			const FPCGExConvexDecompositionDetails& Settings = FPCGExConvexDecompositionDetails());

	protected:
		/**
		 * Check if a set of points is "convex enough"
		 * Returns ratio of interior points (0 = all on hull = perfectly convex)
		 */
		double ComputeConvexityRatio(
			const TArray<FVector>& Positions) const;

		/**
		 * Find the best splitting plane for a set of points
		 */
		bool FindSplitPlane(
			const TArray<FVector>& Positions,
			FVector& OutPlaneOrigin,
			FVector& OutPlaneNormal) const;

		/**
		 * Recursive decomposition
		 */
		void DecomposeRecursive(
			const FCluster* Cluster,
			const TArray<int32>& NodeIndices,
			const FPCGExConvexDecompositionDetails& Settings,
			TArray<FConvexCell3D>& OutCells,
			int32 Depth);

		/**
		 * 3D Convex Hull using gift wrapping / quickhull
		 * Returns indices of points on the hull
		 */
		void ComputeConvexHull(
			const TArray<FVector>& Points,
			TArray<int32>& OutHullIndices) const;
	};
}
