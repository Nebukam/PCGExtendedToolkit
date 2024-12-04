// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Blending/PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	FPropertiesBlender::FPropertiesBlender(const FPCGExPropertiesBlendingDetails& InDetails)
	{
		Init(InDetails);
	}

	void FPropertiesBlender::Init(const FPCGExPropertiesBlendingDetails& InDetails)
	{
		bRequiresPrepare = false;

#define PCGEX_BLEND_FUNCASSIGN(_TYPE, _NAME, _FUNC)\
bReset##_NAME = false; _NAME##Blending = InDetails._NAME##Blending;\
if(ResetBlend[static_cast<uint8>(_NAME##Blending)]){ bReset##_NAME=true; bRequiresPrepare = true; }

		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCASSIGN)
#undef PCGEX_BLEND_FUNCASSIGN

#define PCGEX_BLEND_ASSIGNFUNC(_TYPE, _NAME, ...) switch (_NAME##Blending) {\
default:\
case EPCGExDataBlendingType::None:				_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return O; }; break;\
case EPCGExDataBlendingType::Average:			_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Add(A, B); }; break;\
case EPCGExDataBlendingType::Min:				_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Min(A, B); }; break;\
case EPCGExDataBlendingType::Max:				_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Max(A, B); }; break;\
case EPCGExDataBlendingType::Copy:				_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Copy(A, B); }; break;\
case EPCGExDataBlendingType::Sum:				_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Add(A, B); }; break;\
case EPCGExDataBlendingType::Weight:			_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::WeightedAdd(A, B, W); }; break;\
case EPCGExDataBlendingType::WeightedSum:		_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::WeightedAdd(A, B, W); }; break;\
case EPCGExDataBlendingType::Lerp:				_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Lerp(A, B, W); }; break; \
case EPCGExDataBlendingType::Subtract:			_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Sub(A, B); }; break; \
case EPCGExDataBlendingType::WeightedSubtract:	_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::WeightedSub(A, B, W); }; break;\
case EPCGExDataBlendingType::UnsignedMin:		_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::UnsignedMin(A, B); }; break; \
case EPCGExDataBlendingType::UnsignedMax:		_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::UnsignedMax(A, B); }; break; \
case EPCGExDataBlendingType::AbsoluteMin:		_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::AbsoluteMin(A, B); }; break; \
case EPCGExDataBlendingType::AbsoluteMax:		_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::AbsoluteMax(A, B); }; break; \
case EPCGExDataBlendingType::CopyOther:			_NAME##Func = [](const _TYPE& O, const _TYPE& A, const _TYPE& B, const double W) -> _TYPE{ return PCGExMath::Copy(B, A); }; break; }

		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_ASSIGNFUNC)
#undef PCGEX_BLEND_ASSIGNFUNC

		bHasNoBlending = InDetails.HasNoBlending();
	}

	void FPropertiesBlender::PrepareBlending(FPCGPoint& Target, const FPCGPoint& Default) const
	{
		if (DensityBlending != EPCGExDataBlendingType::None) { Target.Density = bResetDensity ? 0 : Default.Density; }
		if (BoundsMinBlending != EPCGExDataBlendingType::None) { Target.BoundsMin = bResetBoundsMin ? FVector::ZeroVector : Default.BoundsMin; }
		if (BoundsMaxBlending != EPCGExDataBlendingType::None) { Target.BoundsMax = bResetBoundsMax ? FVector::ZeroVector : Default.BoundsMax; }
		if (ColorBlending != EPCGExDataBlendingType::None) { Target.Color = bResetColor ? FVector4::Zero() : Default.Color; }
		if (PositionBlending != EPCGExDataBlendingType::None) { Target.Transform.SetLocation(bResetPosition ? FVector::ZeroVector : Default.Transform.GetLocation()); }
		if (RotationBlending != EPCGExDataBlendingType::None) { Target.Transform.SetRotation(bResetRotation ? FQuat{} : Default.Transform.GetRotation()); }
		if (ScaleBlending != EPCGExDataBlendingType::None) { Target.Transform.SetScale3D(bResetScale ? FVector::ZeroVector : Default.Transform.GetScale3D()); }
		if (SteepnessBlending != EPCGExDataBlendingType::None) { Target.Steepness = bResetSteepness ? 0 : Default.Steepness; }
		if (SeedBlending != EPCGExDataBlendingType::None) { Target.Seed = bResetSeed ? 0 : Default.Seed; }
#if PCGEX_ENGINE_VERSION >= 505
		// TODO : Support new stuff
#endif
	}

	void FPropertiesBlender::Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Weight) const
	{
#define PCGEX_BLEND_PROPDECL(_TYPE, _NAME, _FUNC, _ACCESSOR) const _TYPE Target##_NAME = _NAME##Func(Target._ACCESSOR, A._ACCESSOR, B._ACCESSOR, Weight);
		PCGEX_FOREACH_BLENDINIT_POINTPROPERTY(PCGEX_BLEND_PROPDECL)
#undef PCGEX_BLEND_PROPDECL

		Target.Density = TargetDensity;
		Target.BoundsMin = TargetBoundsMin;
		Target.BoundsMax = TargetBoundsMax;
		Target.Color = TargetColor;
		Target.Transform.SetLocation(TargetPosition);
		Target.Transform.SetRotation(TargetRotation);
		Target.Transform.SetScale3D(TargetScale);
		Target.Steepness = TargetSteepness;
		Target.Seed = TargetSeed;
	}

	void FPropertiesBlender::CompleteBlending(FPCGPoint& Target, const int32 Count, const double TotalWeight) const
	{
#define PCGEX_BLEND_PROPDECL(_TYPE, _NAME, _FUNC, _ACCESSOR) _TYPE Target##_NAME = Target._ACCESSOR;\
		if (_NAME##Blending == EPCGExDataBlendingType::Average) { Target##_NAME = PCGExMath::Div(Target._ACCESSOR, Count); }\
		else if (_NAME##Blending == EPCGExDataBlendingType::Weight) { Target##_NAME = PCGExMath::Div(Target._ACCESSOR, TotalWeight); }
		PCGEX_FOREACH_BLENDINIT_POINTPROPERTY(PCGEX_BLEND_PROPDECL)
#undef PCGEX_BLEND_PROPDECL

		Target.Density = TargetDensity;
		Target.BoundsMin = TargetBoundsMin;
		Target.BoundsMax = TargetBoundsMax;
		Target.Color = TargetColor;
		Target.Transform.SetLocation(TargetPosition);
		Target.Transform.SetRotation(TargetRotation);
		Target.Transform.SetScale3D(TargetScale);
		Target.Steepness = TargetSteepness;
		Target.Seed = TargetSeed;
	}

	void FPropertiesBlender::BlendOnce(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, const double Weight) const
	{
		if (bRequiresPrepare)
		{
			PrepareBlending(Target, A);
			Blend(A, B, Target, Weight);
			CompleteBlending(Target, 2, 1);
		}
		else
		{
			Blend(A, B, Target, Weight);
		}
	}

	void FPropertiesBlender::PrepareRangeBlending(const TArrayView<FPCGPoint>& Targets, const FPCGPoint& Default) const
	{
		for (FPCGPoint& Target : Targets) { PrepareBlending(Target, Default); }
	}

	void FPropertiesBlender::BlendRange(const FPCGPoint& From, const FPCGPoint& To, const TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Weights) const
	{
		for (int i = 0; i < Targets.Num(); i++) { Blend(From, To, Targets[i], Weights[i]); }
	}

	void FPropertiesBlender::CompleteRangeBlending(const TArrayView<FPCGPoint>& Targets, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const
	{
		for (int i = 0; i < Targets.Num(); i++) { CompleteBlending(Targets[i], Counts[i], TotalWeights[i]); }
	}

	void FPropertiesBlender::BlendRangeFromTo(const FPCGPoint& From, const FPCGPoint& To, const TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Weights) const
	{
		if (bRequiresPrepare)
		{
			for (int i = 0; i < Targets.Num(); i++)
			{
				FPCGPoint& Target = Targets[i];
				PrepareBlending(Target, From);
				Blend(From, To, Target, Weights[i]);
				CompleteBlending(Target, 2, 1);
			}
		}
		else
		{
			BlendRange(From, To, Targets, Weights);
		}
	}
}
