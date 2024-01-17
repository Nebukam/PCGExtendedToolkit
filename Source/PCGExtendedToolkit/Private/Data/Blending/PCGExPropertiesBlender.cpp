// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Blending/PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	FPropertiesBlender::FPropertiesBlender(const FPCGExBlendingSettings& Settings)
	{
		Init(Settings);
	}

	void FPropertiesBlender::Init(const FPCGExBlendingSettings& BlendingSettings)
	{
		Init(BlendingSettings.PropertiesOverrides, BlendingSettings.DefaultBlending);
	}

	void FPropertiesBlender::Init(const FPCGExPointPropertyBlendingOverrides& BlendingOverrides, const EPCGExDataBlendingType InDefaultBlending)
	{
		DefaultBlending = InDefaultBlending;
		bRequiresPrepare = false;

#define PCGEX_BLEND_FUNCASSIGN(_TYPE, _NAME, _FUNC)\
bAverage##_NAME = false;\
_NAME##Blending = BlendingOverrides.bOverride##_NAME ? BlendingOverrides._NAME##Blending : DefaultBlending;\
if(_NAME##Blending == EPCGExDataBlendingType::Average){bAverage##_NAME=true; bRequiresPrepare = true;}

		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCASSIGN)
#undef PCGEX_BLEND_FUNCASSIGN
	}

	void FPropertiesBlender::PrepareBlending(FPCGPoint& Target, const FPCGPoint& Default) const
	{
		Target.Density = bAverageDensity ? 0 : Default.Density;
		Target.BoundsMin = bAverageBoundsMin ? FVector::ZeroVector : Default.BoundsMin;
		Target.BoundsMax = bAverageBoundsMax ? FVector::ZeroVector : Default.BoundsMax;
		Target.Color = bAverageColor ? FVector4::Zero() : Default.Color;
		Target.Transform.SetLocation(bAveragePosition ? FVector::ZeroVector : Default.Transform.GetLocation());
		Target.Transform.SetRotation(bAverageRotation ? FQuat{} : Default.Transform.GetRotation());
		Target.Transform.SetScale3D(bAverageScale ? FVector::ZeroVector : Default.Transform.GetScale3D());
		Target.Steepness = bAverageSteepness ? 0 : Default.Steepness;
		Target.Seed = bAverageSeed ? 0 : Default.Seed;
	}

	void FPropertiesBlender::Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Alpha) const
	{
#define PCGEX_BLEND_PROPDECL(_TYPE, _NAME, _FUNC, _ACCESSOR)\
_TYPE Target##_NAME = Target._ACCESSOR;\
switch (_NAME##Blending) {\
case EPCGExDataBlendingType::None:		Target##_NAME = PCGExMath::NoBlend(A._ACCESSOR, B._ACCESSOR, Alpha); break;\
case EPCGExDataBlendingType::Average:	Target##_NAME = PCGExMath::Add(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
case EPCGExDataBlendingType::Weight:	Target##_NAME = PCGExMath::Lerp(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
case EPCGExDataBlendingType::Min:		Target##_NAME = PCGExMath::Min(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
case EPCGExDataBlendingType::Max:		Target##_NAME = PCGExMath::Max(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
case EPCGExDataBlendingType::Copy:		Target##_NAME = PCGExMath::Copy(A._ACCESSOR, B._ACCESSOR, Alpha);break;}

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

	void FPropertiesBlender::CompleteBlending(FPCGPoint& Target, const double Alpha) const
	{
		if (bAverageDensity) { Target.Density = PCGExMath::Div(Target.Density, Alpha); }
		if (bAverageBoundsMin) { Target.BoundsMin = PCGExMath::Div(Target.BoundsMin, Alpha); }
		if (bAverageBoundsMax) { Target.BoundsMax = PCGExMath::Div(Target.BoundsMax, Alpha); }
		if (bAverageColor) { Target.Color = PCGExMath::Div(Target.Color, Alpha); }
		if (bAveragePosition) { Target.Transform.SetLocation(PCGExMath::Div(Target.Transform.GetLocation(), Alpha)); }
		if (bAverageRotation) { Target.Transform.SetRotation(PCGExMath::Div(Target.Transform.GetRotation(), Alpha)); }
		if (bAverageScale) { Target.Transform.SetScale3D(PCGExMath::Div(Target.Transform.GetScale3D(), Alpha)); }
		if (bAverageSteepness) { Target.Steepness = PCGExMath::Div(Target.Steepness, Alpha); }
		if (bAverageSeed) { Target.Seed = PCGExMath::Div(Target.Seed, Alpha); }
	}

	void FPropertiesBlender::BlendOnce(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, const double Alpha) const
	{
		if (bRequiresPrepare)
		{
			PrepareBlending(Target, A);
			Blend(A, B, Target, Alpha);
			CompleteBlending(Target, 2);
		}
		else
		{
			Blend(A, B, Target, Alpha);
		}
	}

	void FPropertiesBlender::PrepareRangeBlending(const TArrayView<FPCGPoint>& Targets, const FPCGPoint& Default) const
	{
		for (FPCGPoint& Target : Targets) { PrepareBlending(Target, Default); }
	}

	void FPropertiesBlender::BlendRange(const FPCGPoint& From, const FPCGPoint& To, const TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Alphas) const
	{
		for (int i = 0; i < Targets.Num(); i++) { Blend(From, To, Targets[i], Alphas[i]); }
	}

	void FPropertiesBlender::CompleteRangeBlending(const TArrayView<FPCGPoint>& Targets, const double Alpha) const
	{
		for (FPCGPoint& Target : Targets) { CompleteBlending(Target, Alpha); }
	}

	void FPropertiesBlender::BlendRangeOnce(const FPCGPoint& A, const FPCGPoint& B, const TArrayView<FPCGPoint>& Targets, const TArrayView<double>& Alphas) const
	{
		if (bRequiresPrepare)
		{
			for (int i = 0; i < Targets.Num(); i++)
			{
				FPCGPoint& Target = Targets[i];
				PrepareBlending(Target, A);
				Blend(A, B, Target, Alphas[i]);
				CompleteBlending(Target, 2);
			}
		}
		else
		{
			BlendRange(A, B, Targets, Alphas);
		}
	}
}
