// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/Blending/PCGExPropertiesBlender.h"

#define PCGEX_FOREACH_BLEND_POINTPROPERTY(MACRO)\
MACRO(float, Density, Float) \
MACRO(FVector, BoundsMin, Vector) \
MACRO(FVector, BoundsMax, Vector) \
MACRO(FVector4, Color, Vector4) \
MACRO(FVector, Position, Vector) \
MACRO(FQuat, Rotation, Quaternion) \
MACRO(FVector, Scale, Vector) \
MACRO(float, Steepness, Float) \
MACRO(int32, Seed, Integer32)

#define PCGEX_FOREACH_BLENDINIT_POINTPROPERTY(MACRO)\
MACRO(float, Density, Float, Density) \
MACRO(FVector, BoundsMin, Vector, BoundsMin) \
MACRO(FVector, BoundsMax, Vector, BoundsMax) \
MACRO(FVector4, Color, Vector4, Color) \
MACRO(FVector, Position, Vector,Transform.GetLocation()) \
MACRO(FQuat, Rotation, Quaternion,Transform.GetRotation()) \
MACRO(FVector, Scale, Vector, Transform.GetScale3D()) \
MACRO(float, Steepness, Float,Steepness) \
MACRO(int32, Seed, Integer32,Seed)

void PCGExDataBlending::FPropertiesBlender::Init(const FPCGExBlendingSettings& Settings)
{
	Init(Settings.PropertiesOverrides, Settings.DefaultBlending);
}

void PCGExDataBlending::FPropertiesBlender::Init(const FPCGExPointPropertyBlendingOverrides& Blendings, EPCGExDataBlendingType InDefaultBlending)
{
	DefaultBlending = InDefaultBlending;
	bRequiresPrepare = false;
	NumBlends = 1;

#define PCGEX_BLEND_FUNCASSIGN(_TYPE, _NAME, _FUNC)\
bAverage##_NAME = false;\
_NAME##Blending = Blendings.bOverride##_NAME ? Blendings._NAME##Blending : DefaultBlending;\
if(_NAME##Blending == EPCGExDataBlendingType::Average){bAverage##_NAME=true; bRequiresPrepare = true;}

	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCASSIGN)
#undef PCGEX_BLEND_FUNCASSIGN
}

void PCGExDataBlending::FPropertiesBlender::PrepareBlending(FPCGPoint& Target, const FPCGPoint& Source) const
{
	Target.Density = bAverageDensity ? 0 : Source.Density;
	Target.BoundsMin = bAverageBoundsMin ? FVector::ZeroVector : Source.BoundsMin;
	Target.BoundsMax = bAverageBoundsMax ? FVector::ZeroVector : Source.BoundsMax;
	Target.Color = bAverageColor ? FVector4::Zero() : Source.Color;
	Target.Transform.SetLocation(bAveragePosition ? FVector::ZeroVector : Source.Transform.GetLocation());
	Target.Transform.SetRotation(bAverageRotation ? FQuat::Identity : Source.Transform.GetRotation());
	Target.Transform.SetScale3D(bAverageScale ? FVector::ZeroVector : Source.Transform.GetScale3D());
	Target.Steepness = bAverageSteepness ? 0 : Source.Steepness;
	Target.Seed = bAverageSeed ? 0 : Source.Seed;
}

void PCGExDataBlending::FPropertiesBlender::Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Alpha)
{
#define PCGEX_BLEND_PROPDECL(_TYPE, _NAME, _FUNC, _ACCESSOR)\
	_TYPE Target##_NAME = Target._ACCESSOR;\
	switch (_NAME##Blending) {\
	case EPCGExDataBlendingType::None:		Target##_NAME = PCGExDataBlending::NoBlend(A._ACCESSOR, B._ACCESSOR, Alpha); break;\
	case EPCGExDataBlendingType::Average:	Target##_NAME = PCGExDataBlending::Add(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
	case EPCGExDataBlendingType::Weight:	Target##_NAME = PCGExDataBlending::Lerp(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
	case EPCGExDataBlendingType::Min:		Target##_NAME = PCGExDataBlending::Min(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
	case EPCGExDataBlendingType::Max:		Target##_NAME = PCGExDataBlending::Max(A._ACCESSOR, B._ACCESSOR, Alpha);break;\
	case EPCGExDataBlendingType::Copy:		Target##_NAME = PCGExDataBlending::Copy(A._ACCESSOR, B._ACCESSOR, Alpha);break;}

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

	NumBlends++;
}

void PCGExDataBlending::FPropertiesBlender::BlendSingle(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Alpha)
{
	if (bRequiresPrepare)
	{
		PrepareBlending(Target, A);
		Blend(A, B, Target, Alpha);
		CompleteBlending(Target);
	}
	else
	{
		Blend(A, B, Target, Alpha);
	}
}

void PCGExDataBlending::FPropertiesBlender::CompleteBlending(FPCGPoint& Target)
{
	if (bAverageDensity) { Target.Density = DivFloat(Target.Density, NumBlends); }
	if (bAverageBoundsMin) { Target.BoundsMin = DivVector(Target.BoundsMin, NumBlends); }
	if (bAverageBoundsMax) { Target.BoundsMax = DivVector(Target.BoundsMax, NumBlends); }
	if (bAverageColor) { Target.Color = DivVector4(Target.Color, NumBlends); }
	if (bAveragePosition) { Target.Transform.SetLocation(DivVector(Target.Transform.GetLocation(), NumBlends)); }
	if (bAverageRotation) { Target.Transform.SetRotation(DivQuaternion(Target.Transform.GetRotation(), NumBlends)); }
	if (bAverageScale) { Target.Transform.SetScale3D(DivVector(Target.Transform.GetScale3D(), NumBlends)); }
	if (bAverageSteepness) { Target.Steepness = DivFloat(Target.Steepness, NumBlends); }
	if (bAverageSeed) { Target.Seed = DivInteger32(Target.Seed, NumBlends); }
	NumBlends = 1;
}

#undef PCGEX_FOREACH_BLEND_POINTPROPERTY
#undef PCGEX_FOREACH_BLENDINIT_POINTPROPERTY
