// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBitmaskFilter.h"

PCGExPointFilter::TFilter* UPCGExBitmaskFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TBitmaskFilter(this);
}

bool PCGExPointsFilter::TBitmaskFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!TFilter::Init(InContext, InPointDataFacade)) { return false; }

	ValueReader = PointDataFacade->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.Value);

	if (!ValueReader)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Value attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Value)));
		return false;
	}

	if (TypedFilterFactory->Descriptor.MaskType == EPCGExFetchType::Attribute)
	{
		MaskReader = PointDataFacade->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.MaskAttribute);
		if (!MaskReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Mask attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.MaskAttribute)));
			return false;
		}
	}

	return true;
}

bool PCGExPointsFilter::TBitmaskFilter::Test(const int32 PointIndex) const
{
	return PCGExCompare::Compare(
		TypedFilterFactory->Descriptor.Comparison,
		ValueReader->Values[PointIndex],
		MaskReader ? MaskReader->Values[PointIndex] : CompositeMask);
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Bitmask)

#if WITH_EDITOR
FString UPCGExBitmaskFilterProviderSettings::GetDisplayName() const
{
	FString A = Descriptor.MaskType == EPCGExFetchType::Attribute ? Descriptor.MaskAttribute.ToString() : TEXT("(Const)");
	FString B = Descriptor.Value.ToString();
	FString DisplayName;

	switch (Descriptor.Comparison)
	{
	case EPCGExBitflagComparison::ContainsAny:
		DisplayName = TEXT("A & B != 0");
		//DisplayName = A + " & " + B + TEXT(" != 0");
		break;
	case EPCGExBitflagComparison::ContainsAll:
		//DisplayName = A + " Any " + B + TEXT(" == B");
		DisplayName = TEXT("A & B == B");
		break;
	case EPCGExBitflagComparison::IsExactly:
		//DisplayName = A + " == " + B;
		DisplayName = TEXT("A == B");
		break;
	case EPCGExBitflagComparison::NotContainsAny:
		//DisplayName = A + " & " + B + TEXT(" == 0");
		DisplayName = TEXT("A & B == 0");
		break;
	case EPCGExBitflagComparison::NotContainsAll:
		//DisplayName = A + " & " + B + TEXT(" != B");
		DisplayName = TEXT("A & B != B");
		break;
	default:
		DisplayName = " ?? ";
		break;
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
