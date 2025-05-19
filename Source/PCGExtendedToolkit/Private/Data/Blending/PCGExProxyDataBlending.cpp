// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExProxyDataBlending.h"

namespace PCGExDataBlending
{
	void FProxyDataBlender::Blend(TArrayView<const int32> SourceIndices, const int32 TargetIndex, TArrayView<const double> Weights)
	{
		check(SourceIndices.Num() ==Weights.Num());

		PCGEx::FOpStats Tracking = BeginMultiBlend(TargetIndex);

		for (int i = 0; i < SourceIndices.Num(); i++)
		{
			const int32 SourceIndex = SourceIndices[i];
			MultiBlend(SourceIndex, TargetIndex, Weights[i], Tracking);
		}

		EndMultiBlend(TargetIndex, Tracking);
	}

	void FProxyDataBlender::Blend(TArrayView<const int32> SourceIndices, const int32 TargetIndex, const double Weight)
	{
		PCGEx::FOpStats Tracking = BeginMultiBlend(TargetIndex);

		for (int i = 0; i < SourceIndices.Num(); i++)
		{
			const int32 SourceIndex = SourceIndices[i];
			MultiBlend(SourceIndex, TargetIndex, Weight, Tracking);
		}

		EndMultiBlend(TargetIndex, Tracking);
	}
}
