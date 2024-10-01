// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExStringCompareFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::TFilter> UPCGExStringCompareFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TStringCompareFilter>(this);
}

bool PCGExPointsFilter::TStringCompareFilter::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = MakeUnique<PCGEx::TAttributeBroadcaster<FString>>();
	if (!OperandA->Prepare(TypedFilterFactory->Config.OperandA, PointDataFacade->Source))
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Config.OperandA)));
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = MakeUnique<PCGEx::TAttributeBroadcaster<FString>>();
		if (!OperandB->Prepare(TypedFilterFactory->Config.OperandB, PointDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Config.OperandB)));
			return false;
		}
	}

	return true;
}

PCGEX_CREATE_FILTER_FACTORY(StringCompare)

#if WITH_EDITOR
FString UPCGExStringCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Config.OperandA.ToString();
	DisplayName += PCGExCompare::ToString(Config.Comparison);
	DisplayName += Config.CompareAgainst == EPCGExFetchType::Constant ? Config.OperandBConstant : Config.OperandB.ToString();
	return DisplayName;
}
#endif


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
