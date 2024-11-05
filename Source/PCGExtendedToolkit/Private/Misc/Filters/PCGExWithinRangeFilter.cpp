// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExWithinRangeFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExWithinRangeFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TWithinRangeFilter>(this);
}

bool PCGExPointsFilter::TWithinRangeFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.OperandA);

	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	RealMin = FMath::Min(TypedFilterFactory->Config.RangeMin, TypedFilterFactory->Config.RangeMax);
	RealMax = FMath::Max(TypedFilterFactory->Config.RangeMin, TypedFilterFactory->Config.RangeMax);

	bInclusive = TypedFilterFactory->Config.bInclusive;
	bInvert = TypedFilterFactory->Config.bInvert;

	return true;
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
