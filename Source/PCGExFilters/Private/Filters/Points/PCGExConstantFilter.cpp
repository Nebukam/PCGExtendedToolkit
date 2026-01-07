// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExConstantFilter.h"

#include "Containers/PCGExManagedObjects.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExConstantFilterFactory::Init(FPCGExContext* InContext)
{
	return Super::Init(InContext);
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExConstantFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointFilter::FConstantFilter, this)
	return Filter;
}

bool PCGExPointFilter::FConstantFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }
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

bool PCGExPointFilter::FConstantFilter::Test(const PCGExData::FProxyPoint& Point) const
{
	return ConstantValue;
}

PCGEX_CREATE_FILTER_FACTORY(Constant)

#if WITH_EDITOR
FString UPCGExConstantFilterProviderSettings::GetDisplayName() const
{
	return Config.Value ? TEXT("Pass") : TEXT("Fail");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
