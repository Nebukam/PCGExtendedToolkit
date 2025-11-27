// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExValueHash.h"

#include "PCGExCommon.h"

namespace PCGExBlend
{
	template <typename T>
	PCGExValueHash ValueHash(const T& Value)
	{
		if constexpr (std::is_same_v<T, FRotator>)
		{
			return GetTypeHash(Value.Euler());
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return HashCombineFast(
				HashCombineFast(GetTypeHash(Value.GetRotation().Euler()), GetTypeHash(Value.GetLocation())),
				GetTypeHash(Value.GetScale3D()));
		}
		else
		{
			return GetTypeHash(Value);
		}
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API PCGExValueHash ValueHash<_TYPE>(const _TYPE& A);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
