// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExTensorDotFilter.h"


#define LOCTEXT_NAMESPACE "PCGExTensorDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExTensorDotFilterDefinition

bool UPCGExTensorDotFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	Config.Sanitize();
	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExTensorDotFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::TTensorDotFilter>(this);
}

bool UPCGExTensorDotFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.DotComparisonDetails.DotValue == EPCGExInputValueType::Attribute, Config.DotComparisonDetails.DotOrDegreesAttribute, Consumable)

	return true;
}

bool PCGExPointsFilter::TTensorDotFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetScopedBroadcaster<FVector>(TypedFilterFactory->Config.OperandA);
	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	// TODO : Validate tensor factories

	return true;
}

PCGEX_CREATE_FILTER_FACTORY(TensorDot)

#if WITH_EDITOR
FString UPCGExTensorDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + " . Tensor";
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
