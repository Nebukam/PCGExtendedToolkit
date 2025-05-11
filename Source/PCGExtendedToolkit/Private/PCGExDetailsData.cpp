// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDetailsData.h"

bool FPCGExInfluenceDetails::Init(const FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
{
	InfluenceBuffer = GetValueSettingInfluence();
	return InfluenceBuffer->Init(InContext, InPointDataFacade, false);
}

bool FPCGExFuseDetailsBase::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!bComponentWiseTolerance) { Tolerances = FVector(Tolerance); }

	if (!InDataFacade)
	{
		ToleranceGetter = PCGExDetails::MakeSettingValue<FVector>(Tolerances);
	}
	else
	{
		ToleranceGetter = PCGExDetails::MakeSettingValue<FVector>(ToleranceInput, ToleranceAttribute, Tolerances);
	}

	if (!ToleranceGetter->Init(InContext, InDataFacade)) { return false; }

	return true;
}

bool FPCGExFuseDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!FPCGExFuseDetailsBase::Init(InContext, InDataFacade)) { return false; }

	DistanceDetails = PCGExDetails::MakeDistances(SourceDistance, TargetDistance);

	return true;
}
