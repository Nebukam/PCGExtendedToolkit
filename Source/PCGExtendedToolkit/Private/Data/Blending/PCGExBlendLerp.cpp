// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExBlendLerp.h"

namespace PCGExBlend
{
	
	template <typename T>
	T Lerp(const T& A, const T& B, const double& W)
	{
		if constexpr (std::is_same_v<T, FQuat>)
		{
			return FQuat::Slerp(A, B, W);
		}
		else if constexpr (std::is_same_v<T, FColor>)
		{
			return FMath::Lerp(A.ReinterpretAsLinear(), B.ReinterpretAsLinear(), W).ToFColor(false);
		}
		else if constexpr (std::is_same_v<T, FRotator>)
		{
			return FRotator(Lerp(A.Pitch, B.Pitch, W), Lerp(A.Yaw, B.Yaw, W), Lerp(A.Roll, B.Roll, W));
		}
		else if constexpr (std::is_same_v<T, FTransform>)
		{
			return FTransform(Lerp(A.GetRotation(), B.GetRotation(), W).GetNormalized(), Lerp(A.GetLocation(), B.GetLocation(), W), Lerp(A.GetScale3D(), B.GetScale3D(), W));
		}
		else if constexpr (
			std::is_same_v<T, bool> ||
			std::is_same_v<T, FString> ||
			std::is_same_v<T, FName> ||
			std::is_same_v<T, FSoftObjectPath> ||
			std::is_same_v<T, FSoftClassPath>)
		{
			return W > 0.5 ? B : A;
		}
		else
		{
			return FMath::Lerp(A, B, W);
		}
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API _TYPE Lerp<_TYPE>(const _TYPE& A, const _TYPE& B, const double& W); 
	
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}
