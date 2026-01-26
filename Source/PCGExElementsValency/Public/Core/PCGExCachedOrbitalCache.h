// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExClusterCache.h"
#include "Core/PCGExValencyOrbitalCache.h"

namespace PCGExValency
{
	/**
	 * Cached orbital cache wrapper for cluster storage.
	 * Allows OrbitalCache to be shared between processors on the same cluster.
	 */
	class PCGEXELEMENTSVALENCY_API FCachedOrbitalCache : public PCGExClusters::ICachedClusterData
	{
	public:
		TSharedPtr<FOrbitalCache> OrbitalCache;
		FName LayerName;  // OrbitalSet layer name for context identification
	};

	/**
	 * Factory for opportunistic orbital cache.
	 * Orbital caches are built by processors at runtime (not PreBuild)
	 * because they depend on vertex/edge attributes.
	 */
	class PCGEXELEMENTSVALENCY_API FOrbitalCacheFactory : public PCGExClusters::IClusterCacheFactory
	{
	public:
		static inline const FName CacheKey = FName("OrbitalCache");

		virtual FName GetCacheKey() const override { return CacheKey; }
		virtual FText GetDisplayName() const override;
		virtual FText GetTooltip() const override;
		virtual EClusterCacheType GetCacheType() const override
		{
			return EClusterCacheType::Opportunistic;
		}

		// Build() not used for opportunistic caches - processors build directly
		virtual TSharedPtr<PCGExClusters::ICachedClusterData> Build(
			const PCGExClusters::FClusterCacheBuildContext& Context) const override
		{
			return nullptr;
		}

		/**
		 * Compute context hash for cache lookup/storage.
		 * @param LayerName The OrbitalSet layer name
		 * @param MaxOrbitals The number of orbitals in the set
		 * @return Hash value for context validation
		 */
		static uint32 ComputeContextHash(FName LayerName, int32 MaxOrbitals);
	};
}
