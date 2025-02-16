// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExConstantFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExConstantFilterFactory::Init(FPCGExContext* InContext)
{
	return Super::Init(InContext);
}

bool UPCGExConstantFilterFactory::SupportsCollectionEvaluation() const
{
	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExConstantFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointFilter::FConstantFilter, this)
	return Filter;
}

bool PCGExPointFilter::FConstantFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }
	ConstantValue = TypedFilterFactory->Config.bInvert ? !TypedFilterFactory->Config.Value : TypedFilterFactory->Config.Value;	
	return true;
}

bool PCGExPointFilter::FConstantFilter::Test(const int32 PointIndex) const
{
	return ConstantValue;
}

bool PCGExPointFilter::FConstantFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	return ConstantValue;
}

PCGEX_CREATE_FILTER_FACTORY(Constant)

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
