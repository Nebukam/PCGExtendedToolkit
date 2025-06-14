// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExProxyDataBlending.h"

namespace PCGExDataBlending
{
	void FDummyUnionBlender::Init(const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources)
	{
		CurrentTargetData = TargetData;

		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { MaxIndex = FMath::Max(Src->Source->IOIndex, MaxIndex); }
		IOLookup = MakeShared<PCGEx::FIndexLookup>(MaxIndex + 1);
		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { IOLookup->Set(Src->Source->IOIndex, SourcesData.Add(Src->GetIn())); }

		Distances = PCGExDetails::MakeDistances();
	}

	int32 FDummyUnionBlender::ComputeWeights(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const
	{
		const PCGExData::FConstPoint Target = CurrentTargetData->Source->GetOutPoint(WriteIndex);
		return InUnionData->ComputeWeights(SourcesData, IOLookup, Target, Distances, OutWeightedPoints);
	}
}
