// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExCachedFaceEnumerator.h"
#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"
#include "Clusters/PCGExCluster.h"
#include "Data/PCGBasePointData.h"
#include "Math/PCGExProjectionDetails.h"

#define LOCTEXT_NAMESPACE "PCGExCachedFaceEnumerator"

namespace PCGExClusters
{
#pragma region FFaceEnumeratorCacheFactory

	FText FFaceEnumeratorCacheFactory::GetDisplayName() const
	{
		return LOCTEXT("DisplayName", "Face Enumerator");
	}

	FText FFaceEnumeratorCacheFactory::GetTooltip() const
	{
		return LOCTEXT("Tooltip", "Pre-built DCEL-based planar face enumerator for cell-finding operations.");
	}

	TSharedPtr<ICachedClusterData> FFaceEnumeratorCacheFactory::Build(const FClusterCacheBuildContext& Context) const
	{
		if (!Context.Projection)
		{
			// No projection settings provided - can't build
			return nullptr;
		}

		// Create projected positions using the projection settings
		TSharedPtr<TArray<FVector2D>> ProjectedPositions = MakeShared<TArray<FVector2D>>();

		// Project cluster vertex positions
		const FCluster& Cluster = Context.Cluster.Get();
		const int32 NumVtx = Cluster.VtxPoints ? Cluster.VtxPoints->GetNumPoints() : 0;

		if (NumVtx <= 0)
		{
			return nullptr;
		}

		ProjectedPositions->SetNumUninitialized(NumVtx);
		Context.Projection->Project(Cluster.VtxTransforms, *ProjectedPositions);

		// Build the face enumerator
		TSharedPtr<FPlanarFaceEnumerator> Enumerator = MakeShared<FPlanarFaceEnumerator>();
		Enumerator->Build(Context.Cluster, *ProjectedPositions);

		if (!Enumerator->IsBuilt())
		{
			return nullptr;
		}

		// Create cached data
		TSharedPtr<FCachedFaceEnumerator> Cached = MakeShared<FCachedFaceEnumerator>();
		Cached->ContextHash = ComputeProjectionHash(*Context.Projection);
		Cached->Enumerator = Enumerator;
		Cached->ProjectedPositions = ProjectedPositions;

		return Cached;
	}

	uint32 FFaceEnumeratorCacheFactory::ComputeProjectionHash(const FPCGExGeo2DProjectionDetails& Projection)
	{
		// Hash the projection settings that affect the 2D layout
		uint32 Hash = GetTypeHash(Projection.Method);
		Hash = HashCombine(Hash, GetTypeHash(Projection.Normal.X));
		Hash = HashCombine(Hash, GetTypeHash(Projection.Normal.Y));
		Hash = HashCombine(Hash, GetTypeHash(Projection.Normal.Z));
		return Hash;
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
