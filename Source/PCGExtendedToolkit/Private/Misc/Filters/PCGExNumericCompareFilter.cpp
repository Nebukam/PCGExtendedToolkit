// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareFilter.h"

PCGExPointFilter::TFilter* UPCGExNumericCompareFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TNumericComparisonFilter(this);
}

bool PCGExPointsFilter::TNumericComparisonFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.OperandA);

	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.OperandB);

		if (!OperandB)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
			return false;
		}
	}

	return true;
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(NumericCompare)

#if WITH_EDITOR
FString UPCGExNumericCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Config.OperandA.GetName().ToString() + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExFetchType::Attribute) { DisplayName += Config.OperandB.GetName().ToString(); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
