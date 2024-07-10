// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBooleanCompareFilter.h"

PCGExPointFilter::TFilter* UPCGExBooleanCompareFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TBooleanComparisonFilter(this);
}

bool PCGExPointsFilter::TBooleanComparisonFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetOrCreateGetter<bool>(TypedFilterFactory->Config.OperandA);

	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = PointDataFacade->GetOrCreateGetter<bool>(TypedFilterFactory->Config.OperandB);

		if (!OperandB)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
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

PCGEX_CREATE_FILTER_FACTORY(BooleanCompare)

#if WITH_EDITOR
FString UPCGExBooleanCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Config.OperandA.GetName().ToString() + (Config.Comparison == EPCGExEquality::Equal ? TEXT(" == ") : TEXT(" != "));

	if (Config.CompareAgainst == EPCGExFetchType::Attribute) { DisplayName += Config.OperandB.GetName().ToString(); }
	else { DisplayName += FString::Printf(TEXT("%s"), Config.OperandBConstant ? TEXT("true") : TEXT("false")); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
