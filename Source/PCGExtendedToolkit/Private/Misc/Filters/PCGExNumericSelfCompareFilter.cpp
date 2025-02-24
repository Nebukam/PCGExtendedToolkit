// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericSelfCompareFilter.h"


#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

TSharedPtr<PCGExPointFilter::FFilter> UPCGExNumericSelfCompareFilterFactory::CreateFilter() const
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
	if (!FFilter::Init(InContext, InPointDataFacade)) { return false; }

	bOffset = TypedFilterFactory->Config.IndexMode == EPCGExIndexMode::Offset;
	MaxIndex = PointDataFacade->Source->GetNum() - 1;

	if (MaxIndex < 0) { return false; }

	OperandA = MakeShared<PCGEx::TAttributeBroadcaster<double>>();

	if (!OperandA->Prepare(TypedFilterFactory->Config.OperandA, PointDataFacade->Source))
	{
		PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Operand A", TypedFilterFactory->Config.OperandA)
		return false;
	}

	if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
	{
		Index = PointDataFacade->GetScopedBroadcaster<int32>(TypedFilterFactory->Config.IndexAttribute);

		if (!Index)
		{
			PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Index", TypedFilterFactory->Config.IndexAttribute)
			return false;
		}
	}

	return true;
}

bool PCGExPointFilter::FNumericSelfCompareFilter::Test(const int32 PointIndex) const
{
	const int32 IndexValue = Index ? Index->Read(PointIndex) : TypedFilterFactory->Config.IndexConstant;
	const int32 TargetIndex = PCGExMath::SanitizeIndex(bOffset ? PointIndex + IndexValue : IndexValue, MaxIndex, TypedFilterFactory->Config.IndexSafety);

	if (TargetIndex == -1) { return false; }

	const double A = OperandA->SoftGet(PointIndex, PointDataFacade->Source->GetInPoint(PointIndex), 0);
	const double B = OperandA->SoftGet(TargetIndex, PointDataFacade->Source->GetInPoint(TargetIndex), 0);
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
