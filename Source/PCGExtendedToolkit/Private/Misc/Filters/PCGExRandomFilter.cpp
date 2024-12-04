// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExRandomFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExRandomFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TRandomFilter>(this);
}

bool PCGExPointsFilter::TRandomFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	return true;
}

PCGEX_CREATE_FILTER_FACTORY(Random)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
