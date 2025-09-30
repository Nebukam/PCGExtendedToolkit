// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsRelax.h"

#include "Details/PCGExDetailsSettings.h"

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExInfluenceDetails::GetValueSettingInfluence(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(InfluenceInput, LocalInfluence, Influence);
	V->bQuietErrors = bQuietErrors;
	return V;
}

bool FPCGExInfluenceDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
{
	InfluenceBuffer = GetValueSettingInfluence();
	return InfluenceBuffer->Init(InPointDataFacade, false);
}

double FPCGExInfluenceDetails::GetInfluence(const int32 PointIndex) const
{
	return InfluenceBuffer->Read(PointIndex);
}
