// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExWithinRangeFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExWithinRangeFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FWithinRangeFilter>(this);
}

bool PCGExPointFilter::FWithinRangeFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandA, true);

	if (!OperandA)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Operand A", TypedFilterFactory->Config.OperandA)
		return false;
	}

	RealMin = FMath::Min(TypedFilterFactory->Config.RangeMin, TypedFilterFactory->Config.RangeMax);
	RealMax = FMath::Max(TypedFilterFactory->Config.RangeMin, TypedFilterFactory->Config.RangeMax);

	bInclusive = TypedFilterFactory->Config.bInclusive;
	bInvert = TypedFilterFactory->Config.bInvert;

	return true;
}

bool PCGExPointFilter::FWithinRangeFilter::Test(const int32 PointIndex) const
{
	if (!bInclusive) { return FMath::IsWithin(OperandA->Read(PointIndex), RealMin, RealMax) ? !bInvert : bInvert; }
	return FMath::IsWithinInclusive(OperandA->Read(PointIndex), RealMin, RealMax) ? !bInvert : bInvert;
}

PCGEX_CREATE_FILTER_FACTORY(WithinRange)

#if WITH_EDITOR
FString UPCGExWithinRangeFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + TEXT("[");

	DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.RangeMin) / 1000.0));
	DisplayName += TEXT(" .. ");
	DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.RangeMax) / 1000.0));

	return DisplayName + TEXT("]");
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
