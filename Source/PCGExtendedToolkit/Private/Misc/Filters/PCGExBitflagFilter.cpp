// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Filters/PCGExBitflagFilter.h"

PCGExDataFilter::TFilter* UPCGExBitflagFilterFactory::CreateFilter() const
{
	return new PCGExPointsFilter::TBitflagFilter(this);
}

void PCGExPointsFilter::TBitflagFilter::Capture(const FPCGContext* InContext, PCGExDataCaching::FPool* InPrimaryDataCache)
{
	TFilter::Capture(InContext, InPrimaryDataCache);

	ValueCache = PointDataCache->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.Value);
	bValid = ValueCache != nullptr;

	CompositeMask = TypedFilterFactory->Descriptor.Mask.GetComposite();
	
	if (!bValid)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Value attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.Value)));
		return;
	}

	if (TypedFilterFactory->Descriptor.MaskType == EPCGExFetchType::Attribute)
	{
		MaskCache = PointDataCache->GetOrCreateReader<int64>(TypedFilterFactory->Descriptor.MaskAttribute);
		bValid = MaskCache != nullptr;

		if (!bValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Mask attribute: {0}."), FText::FromName(TypedFilterFactory->Descriptor.MaskAttribute)));
		}
	}
}

bool PCGExPointsFilter::TBitflagFilter::Test(const int32 PointIndex) const
{
	return PCGExCompare::Compare(
		TypedFilterFactory->Descriptor.Comparison,
		ValueCache->Reader->Values[PointIndex],
		MaskCache ? MaskCache->Values[PointIndex] : CompositeMask);
}

namespace PCGExCompareFilter
{
}

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

PCGEX_CREATE_FILTER_FACTORY(Bitflag)

#if WITH_EDITOR
FString UPCGExBitflagFilterProviderSettings::GetDisplayName() const
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
