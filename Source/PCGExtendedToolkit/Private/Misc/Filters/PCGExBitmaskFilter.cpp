// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBitmaskFilter.h"

PCGExPointFilter::TFilter* UPCGExBitmaskFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TBitmaskFilter(this);
}

bool PCGExPointsFilter::TBitmaskFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache)
{
	if (!TFilter::Init(InContext, InPointDataCache)) { return false; }

	ValueReader = PointDataCache->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.Value);

	if (!ValueReader)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Value attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Value)));
		return false;
	}

	if (TypedFilterFactory->Descriptor.MaskType == EPCGExFetchType::Attribute)
	{
		MaskReader = PointDataCache->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.MaskAttribute);
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
	FString DisplayName = Descriptor.Value.ToString() + PCGExCompare::ToString(Descriptor.Comparison);

	if (Descriptor.MaskType == EPCGExFetchType::Attribute) { DisplayName += Descriptor.MaskAttribute.ToString(); }
	else
	{
		//DisplayName += FString::Printf(TEXT("%lld"), (Descriptor.Mask));
		DisplayName += TEXT("Const");
	}

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
