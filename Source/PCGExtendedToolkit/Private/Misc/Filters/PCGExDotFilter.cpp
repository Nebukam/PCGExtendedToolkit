// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExDotFilter.h"


#define LOCTEXT_NAMESPACE "PCGExDotFilterDefinition"
#define PCGEX_NAMESPACE PCGExDotFilterDefinition

bool UPCGExDotFilterFactory::Init(FPCGExContext* InContext)
{
	if (!Super::Init(InContext)) { return false; }
	Config.Sanitize();
	return true;
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExDotFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FDotFilter>(this);
}

bool UPCGExDotFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.OperandB, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.DotComparisonDetails.ThresholdInput == EPCGExInputValueType::Attribute, Config.DotComparisonDetails.ThresholdAttribute, Consumable)

	return true;
}

bool PCGExPointFilter::FDotFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	OperandA = PointDataFacade->GetScopedBroadcaster<FVector>(TypedFilterFactory->Config.OperandA);
	if (!OperandA)
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Operand A", TypedFilterFactory->Config.OperandA)
		return false;
	}

	OperandB = TypedFilterFactory->Config.GetValueSettingOperandB();
	if (!OperandB->Init(InContext, PointDataFacade)) { return false; }

	return true;
}

bool PCGExPointFilter::FDotFilter::Test(const int32 PointIndex) const
{
	const FPCGPoint& Point = PointDataFacade->Source->GetInPoint(PointIndex);
	const FVector B = OperandB->Read(PointIndex).GetSafeNormal();
	return DotComparison.Test(
		FVector::DotProduct(
			TypedFilterFactory->Config.bTransformOperandA ? Point.Transform.TransformVectorNoScale(OperandA->Read(PointIndex)) : OperandA->Read(PointIndex),
			TypedFilterFactory->Config.bTransformOperandB ? Point.Transform.TransformVectorNoScale(B) : B),
		PointIndex);
}

PCGEX_CREATE_FILTER_FACTORY(Dot)

#if WITH_EDITOR
FString UPCGExDotFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + " ⋅ ";

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.OperandB); }
	else { DisplayName += " (Constant)"; }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
