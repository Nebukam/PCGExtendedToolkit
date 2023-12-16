// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExPropertiesBlender.h"

#define PCGEX_FOREACH_BLEND_POINTPROPERTY(MACRO)\
MACRO(float, Density) \
MACRO(FVector, BoundsMin) \
MACRO(FVector, BoundsMax) \
MACRO(FVector4, Color) \
MACRO(FVector, Position) \
MACRO(FQuat, Rotation) \
MACRO(FVector, Scale) \
MACRO(float, Steepness) \
MACRO(int32, Seed)

namespace PCGExDataBlending
{
	struct PCGEXTENDEDTOOLKIT_API FPropertiesBlender
	{
#define PCGEX_BLEND_FUNCREF(_TYPE, _NAME) bool bAverage##_NAME = false; EPCGExDataBlendingType _NAME##Blending = EPCGExDataBlendingType::Weight;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCREF)
#undef PCGEX_BLEND_FUNCREF

		EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Weight;

		bool bRequiresPrepare = false;
		double NumBlends = 1;

		FPropertiesBlender()
		{
		}

		FPropertiesBlender(const FPCGExBlendingSettings& Settings);

		FPropertiesBlender(const FPropertiesBlender& Other):
#define PCGEX_BLEND_COPY(_TYPE, _NAME) bAverage##_NAME(Other.bAverage##_NAME), _NAME##Blending(Other._NAME##Blending),
			PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_COPY)
#undef PCGEX_BLEND_COPY
			DefaultBlending(Other.DefaultBlending),
			bRequiresPrepare(Other.bRequiresPrepare)
		{
		}

		void Init(const FPCGExBlendingSettings& BlendingSettings);
		void Init(const FPCGExPointPropertyBlendingOverrides& BlendingOverrides, EPCGExDataBlendingType InDefaultBlending);

		void PrepareBlending(FPCGPoint& Target, const FPCGPoint& Source);
		void Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Alpha);
		void CompleteBlending(FPCGPoint& Target);

		void BlendOnce(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Alpha);

		void PrepareRangeBlending(const FPCGPoint& A, const TArrayView<FPCGPoint>& Targets);
		void BlendRange(const FPCGPoint& From, const FPCGPoint& To, TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Alpha);
		void CompleteRangeBlending(const TArrayView<FPCGPoint>& Targets);

		void BlendRangeOnce(const FPCGPoint& A, const FPCGPoint& B, TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Alphas);
	};
}

#undef PCGEX_FOREACH_BLEND_POINTPROPERTY
