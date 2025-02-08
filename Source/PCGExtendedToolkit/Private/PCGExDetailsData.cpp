// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDetailsData.h"

bool FPCGExInfluenceDetails::Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
{
	if (InfluenceInput == EPCGExInputValueType::Attribute)
	{
		InfluenceBuffer = InPointDataFacade->GetBroadcaster<double>(LocalInfluence);
		if (!InfluenceBuffer)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Influence attribute: \"{0}\"."), FText::FromName(LocalInfluence.GetName())));
			return false;
		}
	}
	return true;
}

double FPCGExInfluenceDetails::GetInfluence(const int32 PointIndex) const
{
	return InfluenceBuffer ? InfluenceBuffer->Read(PointIndex) : Influence;
}
