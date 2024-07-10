// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"

bool UPCGExDotFilterFactory::Init(const FPCGContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	Config.Sanitize();
	return true;
}

PCGExPointFilter::TFilter* UPCGExDotFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TDotFilter(this);
}

bool PCGExPointsFilter::TDotFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetOrCreateGetter<FVector>(TypedFilterFactory->Config.OperandA);
	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = PointDataFacade->GetOrCreateGetter<FVector>(TypedFilterFactory->Config.OperandB);
		if (!OperandB)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
			return false;
		}
	}

	return true;
}

#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExDotFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Dot)

#if WITH_EDITOR
FString UPCGExDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Config.OperandA.GetName().ToString() + " . ";

	if (Config.CompareAgainst == EPCGExFetchType::Attribute) { DisplayName += Config.OperandB.GetName().ToString(); }
	else { DisplayName += " (Constant)"; }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
