// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExNumericCompareFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::FNumericCompareFilter>(this);
}

bool UPCGExNumericCompareFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

bool PCGExPointsFilter::FNumericCompareFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.OperandA);

	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
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

PCGEX_CREATE_FILTER_FACTORY(NumericCompare)

#if WITH_EDITOR
FString UPCGExNumericCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
