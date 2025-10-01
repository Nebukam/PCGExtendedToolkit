// Copyright 2025 Timothé Lapetite and contributors
// * 24/02/25 Omer Salomon Changed - Div(const FQuat& A, const double Divider) now converts FQuat to FRotator and calls Div on that.
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "Details/PCGExMacros.h"

namespace PCGExBlend
{
	template <typename T>
	uint32 ValueHash(const T& Value);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template uint32 ValueHash<_TYPE>(const _TYPE& Value);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
