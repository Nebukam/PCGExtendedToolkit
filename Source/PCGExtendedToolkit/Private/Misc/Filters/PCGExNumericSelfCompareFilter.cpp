// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericSelfCompareFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::IFilter> UPCGExNumericSelfCompareFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExPointFilter::FNumericSelfCompareFilter>(this);
}

bool UPCGExNumericSelfCompareFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.OperandA, Consumable)
	PCGEX_CONSUMABLE_CONDITIONAL(Config.CompareAgainst == EPCGExInputValueType::Attribute, Config.IndexAttribute, Consumable)

	return true;
}

bool PCGExPointFilter::FNumericSelfCompareFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }

	bOffset = TypedFilterFactory->Config.IndexMode == EPCGExIndexMode::Offset;
	MaxIndex = PointDataFacade->Source->GetNum() - 1;

	if (MaxIndex < 0) { return false; }

	OperandA = MakeShared<PCGEx::TAttributeBroadcaster<double>>();

	if (!OperandA->Prepare(TypedFilterFactory->Config.OperandA, PointDataFacade->Source))
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, Operand A, TypedFilterFactory->Config.OperandA)
		return false;
	}

	Index = TypedFilterFactory->Config.GetValueSettingIndex();
	if (!Index->Init(InContext, PointDataFacade)) { return false; }

	return true;
}

bool PCGExPointFilter::FNumericSelfCompareFilter::Test(const int32 PointIndex) const
{
	const int32 IndexValue = Index->Read(PointIndex);
	const int32 TargetIndex = PCGExMath::SanitizeIndex(bOffset ? PointIndex + IndexValue : IndexValue, MaxIndex, TypedFilterFactory->Config.IndexSafety);

	if (TargetIndex == -1) { return TypedFilterFactory->Config.InvalidIndexFallback == EPCGExFilterFallback::Pass; }

	const double A = OperandA->SoftGet(PointDataFacade->Source->GetInPoint(PointIndex), 0);
	const double B = OperandA->SoftGet(PointDataFacade->Source->GetInPoint(TargetIndex), 0);
	return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
}

PCGEX_CREATE_FILTER_FACTORY(NumericSelfCompare)

#if WITH_EDITOR
FString UPCGExNumericSelfCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = PCGEx::GetSelectorDisplayName(Config.OperandA) + PCGExCompare::ToString(Config.Comparison);

	if (Config.IndexMode == EPCGExIndexMode::Pick) { DisplayName += TEXT(" @ "); }
	else { DisplayName += TEXT(" i+ "); }

	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { DisplayName += PCGEx::GetSelectorDisplayName(Config.IndexAttribute); }
	else { DisplayName += FString::Printf(TEXT("%d"), Config.IndexConstant); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
