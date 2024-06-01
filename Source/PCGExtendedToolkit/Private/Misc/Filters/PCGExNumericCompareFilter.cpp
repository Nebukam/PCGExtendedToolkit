// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareFilter.h"

PCGExDataFilter::TFilter* UPCGExNumericCompareFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TNumericComparisonFilter(this);
}

void PCGExPointsFilter::TNumericComparisonFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
{
	bValid = true;

	OperandA = new PCGEx::FLocalSingleFieldGetter();
	OperandA->Capture(TypedFilterFactory->OperandA);
	OperandA->Grab(*PointIO, false);
	bValid = OperandA->IsUsable(PointIO->GetNum());

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return;
	}

	if (TypedFilterFactory->CompareAgainst == EPCGExOperandType::Attribute)
	{
		OperandB = new PCGEx::FLocalSingleFieldGetter();
		OperandB->Capture(TypedFilterFactory->OperandB);
		OperandB->Grab(*PointIO, false);
		bValid = OperandB->IsUsable(PointIO->GetNum());

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->OperandB.GetName())));
			PCGEX_DELETE(OperandB)
		}
	}
}

bool PCGExPointsFilter::TNumericComparisonFilter::Test(const int32 PointIndex) const
{
	const double A = OperandA->Values[PointIndex];
	const double B = TypedFilterFactory->CompareAgainst == EPCGExOperandType::Attribute ? OperandB->Values[PointIndex] : TypedFilterFactory->OperandBConstant;
	return PCGExCompare::Compare(TypedFilterFactory->Comparison, A, B, TypedFilterFactory->Tolerance);
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
	FString DisplayName = Descriptor.OperandA.GetName().ToString() + PCGExCompare::ToString(Descriptor.Comparison);

	if (Descriptor.CompareAgainst == EPCGExOperandType::Attribute) { DisplayName += Descriptor.OperandB.GetName().ToString(); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
