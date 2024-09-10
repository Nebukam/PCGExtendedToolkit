// Copyright Timothé Lapetite 2024
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
bReset##_NAME = false;\
_NAME##Blending = InDetails._NAME##Blending;\
if(ResetBlend.Contains(_NAME##Blending)){ bReset##_NAME=true; bRequiresPrepare = true; }

		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_FUNCASSIGN)
#undef PCGEX_BLEND_FUNCASSIGN

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
	}

	void FPropertiesBlender::Blend(const FPCGPoint& A, const FPCGPoint& B, FPCGPoint& Target, double Weight) const
	{
#define PCGEX_BLEND_PROPDECL(_TYPE, _NAME, _FUNC, _ACCESSOR)\
_TYPE Target##_NAME = Target._ACCESSOR;\
switch (_NAME##Blending) {\
case EPCGExDataBlendingType::None:  break;\
case EPCGExDataBlendingType::Average:		Target##_NAME = PCGExMath::Add(A._ACCESSOR, B._ACCESSOR);break;\
case EPCGExDataBlendingType::Min:			Target##_NAME = PCGExMath::Min(A._ACCESSOR, B._ACCESSOR);break;\
case EPCGExDataBlendingType::Max:			Target##_NAME = PCGExMath::Max(A._ACCESSOR, B._ACCESSOR);break;\
case EPCGExDataBlendingType::Copy:			Target##_NAME = PCGExMath::Copy(A._ACCESSOR, B._ACCESSOR);break;\
case EPCGExDataBlendingType::Sum:			Target##_NAME = PCGExMath::Add(A._ACCESSOR, B._ACCESSOR);break;\
case EPCGExDataBlendingType::Weight:		Target##_NAME = PCGExMath::WeightedAdd(A._ACCESSOR, B._ACCESSOR, Weight);break;\
case EPCGExDataBlendingType::WeightedSum:	Target##_NAME = PCGExMath::WeightedAdd(A._ACCESSOR, B._ACCESSOR, Weight);break;\
case EPCGExDataBlendingType::Lerp:			Target##_NAME = PCGExMath::Lerp(A._ACCESSOR, B._ACCESSOR, Weight);break; \
case EPCGExDataBlendingType::Subtract:		Target##_NAME = PCGExMath::Subtract(A._ACCESSOR, B._ACCESSOR);break; }

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
		if (DensityBlending == EPCGExDataBlendingType::Average) { Target.Density = PCGExMath::Div(Target.Density, Count); }
		else if (DensityBlending == EPCGExDataBlendingType::Weight) { Target.Density = PCGExMath::Div(Target.Density, TotalWeight); }

		if (BoundsMinBlending == EPCGExDataBlendingType::Average) { Target.BoundsMin = PCGExMath::Div(Target.BoundsMin, Count); }
		else if (BoundsMinBlending == EPCGExDataBlendingType::Weight) { Target.BoundsMin = PCGExMath::Div(Target.BoundsMin, TotalWeight); }

		if (BoundsMaxBlending == EPCGExDataBlendingType::Average) { Target.BoundsMax = PCGExMath::Div(Target.BoundsMax, Count); }
		else if (BoundsMaxBlending == EPCGExDataBlendingType::Weight) { Target.BoundsMax = PCGExMath::Div(Target.BoundsMax, TotalWeight); }

		if (ColorBlending == EPCGExDataBlendingType::Average) { Target.Color = PCGExMath::Div(Target.Color, Count); }
		else if (ColorBlending == EPCGExDataBlendingType::Weight) { Target.Color = PCGExMath::Div(Target.Color, TotalWeight); }

		if (PositionBlending == EPCGExDataBlendingType::Average) { Target.Transform.SetLocation(PCGExMath::Div(Target.Transform.GetLocation(), Count)); }
		else if (PositionBlending == EPCGExDataBlendingType::Weight) { Target.Transform.SetLocation(PCGExMath::Div(Target.Transform.GetLocation(), TotalWeight)); }

		if (RotationBlending == EPCGExDataBlendingType::Average) { Target.Transform.SetRotation(PCGExMath::Div(Target.Transform.GetRotation(), Count)); }
		else if (RotationBlending == EPCGExDataBlendingType::Weight) { Target.Transform.SetRotation(PCGExMath::Div(Target.Transform.GetRotation(), TotalWeight)); }

		if (ScaleBlending == EPCGExDataBlendingType::Average) { Target.Transform.SetScale3D(PCGExMath::Div(Target.Transform.GetScale3D(), Count)); }
		else if (ScaleBlending == EPCGExDataBlendingType::Weight) { Target.Transform.SetScale3D(PCGExMath::Div(Target.Transform.GetScale3D(), TotalWeight)); }

		if (SteepnessBlending == EPCGExDataBlendingType::Average) { Target.Steepness = PCGExMath::Div(Target.Steepness, Count); }
		else if (SteepnessBlending == EPCGExDataBlendingType::Weight) { Target.Steepness = PCGExMath::Div(Target.Steepness, TotalWeight); }

		if (SeedBlending == EPCGExDataBlendingType::Average) { Target.Seed = PCGExMath::Div(Target.Seed, Count); }
		else if (SeedBlending == EPCGExDataBlendingType::Weight) { Target.Seed = PCGExMath::Div(Target.Seed, TotalWeight); }
	}

	void FPropertiesBlender::CopyBlendedProperties(FPCGPoint& Target, const FPCGPoint& Source) const
	{
#define PCGEX_BLEND_COPYTO(_TYPE, _NAME, _FUNC, _ACCESSOR)\
if (_NAME##Blending != EPCGExDataBlendingType::None){ Target._ACCESSOR = Source._ACCESSOR;}
		PCGEX_FOREACH_BLENDINIT_POINTPROPERTY(PCGEX_BLEND_COPYTO)
#undef PCGEX_BLEND_COPYTO
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
