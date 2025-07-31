// Copyright 2025 Timothé Lapetite and contributors
// * 24/02/25 Omer Salomon Changed - Div(const FQuat& A, const double Divider) now converts FQuat to FRotator and calls Div on that.
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExH.h"

namespace PCGExBlend
{
	template <typename T>
	T Min(const T& A, const T& B);

	template <typename T>
	T Max(const T& A, const T& B);

	template <typename T>
	T Add(const T& A, const T& B);

	template <typename T>
	T ModSimple(const T& A, const double Modulo);

	template <typename T>
	T ModComplex(const T& A, const T& B);

	template <typename T>
	T WeightedAdd(const T& A, const T& B, const double& W = 0);

	template <typename T>
	T Sub(const T& A, const T& B, const double& W = 0);

	template <typename T>
	T WeightedSub(const T& A, const T& B, const double& W = 0);

	template <typename T>
	T UnsignedMin(const T& A, const T& B);

	template <typename T>
	T UnsignedMax(const T& A, const T& B);

	template <typename T>
	T AbsoluteMin(const T& A, const T& B);

	template <typename T>
	T AbsoluteMax(const T& A, const T& B);

	template <typename T>
	T Lerp(const T& A, const T& B, const double& W = 0);

	template <typename T>
	T Div(const T& A, const double Divider);

	template <typename T>
	T Mult(const T& A, const T& B);

	template <typename T>
	T Copy(const T& A, const T& B);

	template <typename T>
	T NoBlend(const T& A, const T& B);

	template <typename T>
	T NaiveHash(const T& A, const T& B);

	template <typename T>
	T NaiveUnsignedHash(const T& A, const T& B);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template _TYPE Min(const _TYPE& A, const _TYPE& B); \
extern template _TYPE Max(const _TYPE& A, const _TYPE& B); \
extern template _TYPE Add(const _TYPE& A, const _TYPE& B); \
extern template _TYPE ModSimple(const _TYPE& A, const double Modulo); \
extern template _TYPE ModComplex(const _TYPE& A, const _TYPE& B); \
extern template _TYPE WeightedAdd(const _TYPE& A, const _TYPE& B, const double& W); \
extern template _TYPE Sub(const _TYPE& A, const _TYPE& B, const double& W); \
extern template _TYPE WeightedSub(const _TYPE& A, const _TYPE& B, const double& W); \
extern template _TYPE UnsignedMin(const _TYPE& A, const _TYPE& B); \
extern template _TYPE UnsignedMax(const _TYPE& A, const _TYPE& B); \
extern template _TYPE AbsoluteMin(const _TYPE& A, const _TYPE& B); \
extern template _TYPE AbsoluteMax(const _TYPE& A, const _TYPE& B); \
extern template _TYPE Lerp(const _TYPE& A, const _TYPE& B, const double& W); \
extern template _TYPE Div(const _TYPE& A, const double Divider); \
extern template _TYPE Mult(const _TYPE& A, const _TYPE& B); \
extern template _TYPE Copy(const _TYPE& A, const _TYPE& B); \
extern template _TYPE NoBlend(const _TYPE& A, const _TYPE& B); \
extern template _TYPE NaiveHash(const _TYPE& A, const _TYPE& B); \
extern template _TYPE NaiveUnsignedHash(const _TYPE& A, const _TYPE& B);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
