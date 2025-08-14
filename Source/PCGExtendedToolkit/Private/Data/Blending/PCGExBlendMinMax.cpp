// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExBlendMinMax.h"

namespace PCGExBlend
{
	template <typename T>
	T Min(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D::Min(A, B);
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector::Min(A, B);
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z), FMath::Min(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FColor(FMath::Min(A.R, B.R), FMath::Min(A.G, B.G), FMath::Min(A.B, B.B), FMath::Min(A.A, B.A));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Min(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(FMath::Min(A.Pitch, B.Pitch), FMath::Min(A.Yaw, B.Yaw), FMath::Min(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Min(A.GetRotation(), B.GetRotation()), Min(A.GetLocation(), B.GetLocation()), Min(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A > B ? B : A;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return A.ToString() > B.ToString() ? B : A;
		}
		else
		{
			return FMath::Min(A, B);
		}
	}

	template <typename T>
	T Max(const T& A, const T& B)
	{
		if constexpr (std::is_same_v<T, bool>)
		{
			return A || B;
		}
		else if constexpr (std::is_same_v<T, FVector2D>)
		{
			return FVector2D::Max(A, B);
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			return FVector::Max(A, B);
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			return FVector4(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z), FMath::Max(A.W, B.W));
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FColor(FMath::Max(A.R, B.R), FMath::Max(A.G, B.G), FMath::Max(A.B, B.B), FMath::Max(A.A, B.A));
		}
		else if constexpr (std::is_same_v<T, FQuat>)
		{
			return Max(A.Rotator(), B.Rotator()).Quaternion();
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(FMath::Max(A.Pitch, B.Pitch), FMath::Max(A.Yaw, B.Yaw), FMath::Max(A.Roll, B.Roll));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Max(A.GetRotation(), B.GetRotation()), Max(A.GetLocation(), B.GetLocation()), Max(A.GetScale3D(), B.GetScale3D()));
		}
		else if constexpr (std::is_same_v<T, FString>)
		{
			return A < B ? B : A;
		}
		else if constexpr (
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftClassPath> ||
			std::is_same_v<T, FSoftObjectPath>)
		{
			return A.ToString() < B.ToString() ? B : A;
		}
		else
		{
			return FMath::Max(A, B);
		}
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API _TYPE Min<_TYPE>(const _TYPE& A, const _TYPE& B); \
template PCGEXTENDEDTOOLKIT_API _TYPE Max<_TYPE>(const _TYPE& A, const _TYPE& B); 
	
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
