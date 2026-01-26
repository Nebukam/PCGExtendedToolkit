// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExCachedOrbitalCache.h"

#define LOCTEXT_NAMESPACE "PCGExCachedOrbitalCache"

namespace PCGExValency
{
#pragma region FOrbitalCacheFactory

	FText FOrbitalCacheFactory::GetDisplayName() const
	{
		return LOCTEXT("DisplayName", "Orbital Cache");
	}

	FText FOrbitalCacheFactory::GetTooltip() const
	{
		return LOCTEXT("Tooltip", "Cached orbital-to-neighbor mappings for valency processing.");
	}

	uint32 FOrbitalCacheFactory::ComputeContextHash(FName LayerName, int32 MaxOrbitals)
	{
		uint32 Hash = GetTypeHash(LayerName);
		Hash = HashCombine(Hash, GetTypeHash(MaxOrbitals));
		return Hash;
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
