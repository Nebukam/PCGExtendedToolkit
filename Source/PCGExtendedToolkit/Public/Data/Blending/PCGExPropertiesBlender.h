// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"
#include "UObject/Object.h"

#include "PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	inline bool ResetBlend[] = {
		false, //None            
		true,  //Average         
		true,  //Weight          
		false, //Min             
		false, //Max             
		false, //Copy            
		true,  //Sum             
		true,  //WeightedSum     
		false, //Lerp            
		false, //Subtract        
		false, //UnsignedMin     
		false, //UnsignedMax     
		false, //AbsoluteMin     
		false, //AbsoluteMax     
		true,  //WeightedSubtract		
		false, //CopyOther		
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPropertiesBlender
	{
#define PCGEX_BLEND_FUNCREF(_TYPE, _NAME, ...) \
	bool bReset##_NAME = false; EPCGExDataBlendingType _NAME##Blending = EPCGExDataBlendingType::Weight; \
	using _NAME##FuncDef = std::function<_TYPE(const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W)>; _NAME##FuncDef _NAME##Func;
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCREF)
#undef PCGEX_BLEND_FUNCREF

		EPCGExDataBlendingType DefaultBlending = EPCGExDataBlendingType::Average;

		bool bRequiresPrepare = false;
		bool bHasNoBlending = false;

		FPropertiesBlender()
		{
		}

		explicit FPropertiesBlender(const FPCGExPropertiesBlendingDetails& InDetails);

		FPropertiesBlender(const FPropertiesBlender& Other):
#define PCGEX_BLEND_COPY(_TYPE, _NAME, ...) bReset##_NAME(Other.bReset##_NAME), _NAME##Blending(Other._NAME##Blending),
			PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_COPY)
#undef PCGEX_BLEND_COPY
			DefaultBlending(Other.DefaultBlending),
			bRequiresPrepare(Other.bRequiresPrepare)
		{
		}

		void Init(const FPCGExPropertiesBlendingDetails& InDetails);

		void PrepareBlending(FPCGPoint& Target, const FPCGPoint& Default) const;
		void Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Weight) const;
		void CompleteBlending(FPCGPoint& Target, const int32 Count, const double TotalWeight) const;

		void BlendOnce(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Weight) const;

		void PrepareRangeBlending(const TArrayView<FPCGPoint>& Targets, const FPCGPoint& Default) const;
		void BlendRange(const FPCGPoint& From, const FPCGPoint& To, const TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Weights) const;
		void CompleteRangeBlending(const TArrayView<FPCGPoint>& Targets, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const;

		void BlendRangeFromTo(const FPCGPoint& From, const FPCGPoint& To, const TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Weights) const;
	};
}
