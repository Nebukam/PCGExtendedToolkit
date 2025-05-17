// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExPropertiesBlender.h"

namespace PCGExDataBlending
{
	void FPropertiesBlender::Init(const FPCGExPropertiesBlendingDetails& InDetails)
	{
		bHasNoBlending = InDetails.HasNoBlending();
		if (bHasNoBlending) { return; }

		// Create per-property typed blenders

		TSharedPtr<FValueRangeBlender> Blender;
#define PCGEX_BLEND_MAKE(_NAME, _REALTYPE, _WORKINGTYPE)\
		Blender = CreateBlender<EPCGPointProperties::_NAME, _REALTYPE, _WORKINGTYPE>(InDetails._NAME##Blending);\
		if(Blender){ Blenders.Add(Blender); if(Blender->WantsPreparation()){ PoleBlenders.Add(Blender); } }
		PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_BLEND_MAKE)
#undef PCGEX_BLEND_MAKE

		bRequiresPrepare = !PoleBlenders.IsEmpty();
	}

	void FPropertiesBlender::PrepareBlending(const int32 TargetIndex, const int32 Default) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : PoleBlenders)
		{
			Blender->PrepareBlending(TargetIndex, Default);
		}
	}

	void FPropertiesBlender::Blend(const int32 A, const int32 B, const int32 Target, const double Weight) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : Blenders)
		{
			Blender->Blend(A, B, Target, Weight);
		}
	}

	void FPropertiesBlender::CompleteBlending(const int32 TargetIndex, const int32 Count, const double TotalWeight) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : PoleBlenders)
		{
			Blender->CompleteBlending(TargetIndex, Count, TotalWeight);
		}
	}

	void FPropertiesBlender::PrepareRangeBlending(const TArrayView<const int32>& Targets, const int32 Default) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : PoleBlenders)
		{
			Blender->PrepareRangeBlending(Targets, Default);
		}
	}

	void FPropertiesBlender::BlendRange(const int32 From, const int32 To, const TArrayView<int32>& Targets, const TArrayView<double>& Weights) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : Blenders)
		{
			Blender->BlendRange(From, To, Targets, Weights);
		}
	}

	void FPropertiesBlender::CompleteRangeBlending(const TArrayView<const int32>& Targets, const TArrayView<const int32>& Counts, const TArrayView<double>& TotalWeights) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : PoleBlenders)
		{
			Blender->CompleteRangeBlending(Targets, Counts, TotalWeights);
		}
	}

	void FPropertiesBlender::BlendRangeFromTo(const int32 From, const int32 To, const TArrayView<const int32>& Targets, const TArrayView<const double>& Weights) const
	{
		for (const TSharedPtr<FValueRangeBlender>& Blender : Blenders)
		{
			Blender->BlendRangeFromTo(From, To, Targets, Weights);
		}
	}
}
