// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExNumericCompareFilter.h"

PCGExDataFilter::TFilter* UPCGExNumericCompareFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TNumericComparisonFilter(this);
}

void PCGExPointsFilter::TNumericComparisonFilter::Capture(const FPCGContext* InContext, PCGExDataCaching::FPool* InPrimaryDataCache)
{
	TFilter::Capture(InContext, InPrimaryDataCache);

	OperandA = PointDataCache->GetOrCreateGetter<double>(TypedFilterFactory->Descriptor.OperandA);
	bValid = OperandA != nullptr;

	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
		return;
	}

	if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = PointDataCache->GetOrCreateGetter<double>(TypedFilterFactory->Descriptor.OperandB);
		bValid = OperandB != nullptr;

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
		}
	}
}

bool PCGExPointsFilter::TNumericComparisonFilter::Test(const int32 PointIndex) const
{
	const double A = OperandA->Values[PointIndex];
	const double B = OperandB ? OperandB->Values[PointIndex] : TypedFilterFactory->Descriptor.OperandBConstant;
	return PCGExCompare::Compare(TypedFilterFactory->Descriptor.Comparison, A, B, TypedFilterFactory->Descriptor.Tolerance);
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

	if (Descriptor.CompareAgainst == EPCGExFetchType::Attribute) { DisplayName += Descriptor.OperandB.GetName().ToString(); }
	else { DisplayName += FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Descriptor.OperandBConstant) / 1000.0)); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
