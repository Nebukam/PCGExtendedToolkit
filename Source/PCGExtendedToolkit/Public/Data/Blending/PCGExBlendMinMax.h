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
	T Min(const T& A, const T& B);

	template <typename T>
	T Max(const T& A, const T& B);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template _TYPE Min<_TYPE>(const _TYPE& A, const _TYPE& B); \
extern template _TYPE Max<_TYPE>(const _TYPE& A, const _TYPE& B);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
