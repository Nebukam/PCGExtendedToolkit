// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExInfluenceDetails.h"

#include "Details/PCGExSettingsDetails.h"

PCGEX_SETTING_VALUE_IMPL(FPCGExInfluenceDetails, Influence, double, InfluenceInput, LocalInfluence, Influence)

bool FPCGExInfluenceDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
{
	InfluenceBuffer = GetValueSettingInfluence();
	return InfluenceBuffer->Init(InPointDataFacade, false);
}

double FPCGExInfluenceDetails::GetInfluence(const int32 PointIndex) const
{
	return InfluenceBuffer->Read(PointIndex);
}
