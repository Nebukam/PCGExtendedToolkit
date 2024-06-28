// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExStringCompareFilter.h"

PCGExPointFilter::TFilter* UPCGExStringCompareFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TStringCompareFilter(this);
}

bool PCGExPointsFilter::TStringCompareFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache)
{
	if (!TFilter::Init(InContext, InPointDataCache)) { return false; }

	OperandA = new PCGEx::TFAttributeReader<FString>(TypedFilterFactory->Descriptor.OperandA.GetName());
	if (!OperandA->Bind(InPointDataCache->Source))
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand A attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandA.GetName())));
		PCGEX_DELETE(OperandA)
		return false;
	}

	if (TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute)
	{
		OperandB = new PCGEx::TFAttributeReader<FString>(TypedFilterFactory->Descriptor.OperandB.GetName());
		if (!OperandB->Bind(InPointDataCache->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Operand B attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.OperandB.GetName())));
			PCGEX_DELETE(OperandA)
			PCGEX_DELETE(OperandB)
			return false;
		}
	}

	return true;
}

bool PCGExPointsFilter::TStringCompareFilter::Test(const int32 PointIndex) const
{
	const FString A = OperandA->Values[PointIndex];
	const FString B = TypedFilterFactory->Descriptor.CompareAgainst == EPCGExFetchType::Attribute ? OperandB->Values[PointIndex] : TypedFilterFactory->Descriptor.OperandBConstant;

	switch (TypedFilterFactory->Descriptor.Comparison)
	{
	case EPCGExStringComparison::StrictlyEqual:
		return A == B;
	case EPCGExStringComparison::StrictlyNotEqual:
		return A != B;
	case EPCGExStringComparison::LengthStrictlyEqual:
		return A.Len() == B.Len();
	case EPCGExStringComparison::LengthStrictlyUnequal:
		return A.Len() != B.Len();
	case EPCGExStringComparison::LengthEqualOrGreater:
		return A.Len() >= B.Len();
	case EPCGExStringComparison::LengthEqualOrSmaller:
		return A.Len() <= B.Len();
	case EPCGExStringComparison::StrictlyGreater:
		return A.Len() > B.Len();
	case EPCGExStringComparison::StrictlySmaller:
		return A.Len() < B.Len();
	case EPCGExStringComparison::LocaleStrictlyGreater:
		return A > B;
	case EPCGExStringComparison::LocaleStrictlySmaller:
		return A < B;
	default:
		return false;
	}
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(StringCompare)

#if WITH_EDITOR
FString UPCGExStringCompareFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = Descriptor.OperandA.GetName().ToString();

	switch (Descriptor.Comparison)
	{
	case EPCGExStringComparison::StrictlyEqual:
		DisplayName += " == ";
		break;
	case EPCGExStringComparison::StrictlyNotEqual:
		DisplayName += " != ";
		break;
	case EPCGExStringComparison::LengthStrictlyEqual:
		DisplayName += " L == L ";
		break;
	case EPCGExStringComparison::LengthStrictlyUnequal:
		DisplayName += " L != L ";
		break;
	case EPCGExStringComparison::LengthEqualOrGreater:
		DisplayName += " L >= L ";
		break;
	case EPCGExStringComparison::LengthEqualOrSmaller:
		DisplayName += " L <= L ";
		break;
	case EPCGExStringComparison::StrictlyGreater:
		DisplayName += " L > L ";
		break;
	case EPCGExStringComparison::StrictlySmaller:
		DisplayName += " L < L ";
		break;
	case EPCGExStringComparison::LocaleStrictlyGreater:
		DisplayName += " > ";
		break;
	case EPCGExStringComparison::LocaleStrictlySmaller:
		DisplayName += " < ";
		break;
	default: ;
	}

	DisplayName += Descriptor.OperandB.GetName().ToString();
	return DisplayName;
}
#endif


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
