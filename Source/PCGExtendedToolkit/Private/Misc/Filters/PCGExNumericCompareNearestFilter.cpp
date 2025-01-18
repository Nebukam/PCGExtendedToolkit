// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareNearestFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExNumericCompareNearestFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }

	TargetDataFacade = PCGExData::TryGetSingleFacade(InContext, PCGEx::SourceTargetsLabel, true);
	if (!TargetDataFacade) { return false; }

	return false;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExNumericCompareNearestFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointsFilter::FNumericCompareNearestFilter>(this);
}

bool UPCGExNumericCompareNearestFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)

	return true;
}

void UPCGExNumericCompareNearestFilterFactory::BeginDestroy()
{
	TargetDataFacade.Reset();
	Super::BeginDestroy();
}

bool PCGExPointsFilter::FNumericCompareNearestFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	if (!TargetDataFacade) { return false; }

	Distances = TypedFilterFactory->Config.DistanceDetails.MakeDistances();

	OperandA = TargetDataFacade->GetScopedBroadcaster<double>(TypedFilterFactory->Config.OperandA);

	if (!OperandA)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandA.GetName())));
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
	{
		OperandB = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.OperandB);

		if (!OperandB)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.OperandB.GetName())));
			return false;
		}
	}

	TargetOctree = &TargetDataFacade->GetIn()->GetOctree();
	InPointsStart = TargetDataFacade->GetIn()->GetPoints().GetData();

	return true;
}

TArray<FPCGPinProperties> UPCGExNumericCompareNearestFilterProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, TEXT("Target points to read operand B from"), Required, {})
	return PinProperties;
}

PCGEX_CREATE_FILTER_FACTORY(NumericCompareNearest)

#if WITH_EDITOR
FString UPCGExNumericCompareNearestFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
