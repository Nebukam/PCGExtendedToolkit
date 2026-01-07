// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Points/PCGExRandomRatioFilter.h"

#include "Data/PCGExData.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExCompareFilterDefinition"
#define PCGEX_NAMESPACE CompareFilterDefinition

bool UPCGExRandomRatioFilterFactory::SupportsCollectionEvaluation() const
{
	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExRandomRatioFilterFactory::CreateFilter() const
{
	PCGEX_MAKE_SHARED(Filter, PCGExPointFilter::FRandomRatioFilter, this)
	return Filter;
}

#if WITH_EDITOR
void UPCGExRandomRatioFilterProviderSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_UPDATE_TO_DATA_VERSION(1, 73, 0)
	{
		Config.Random.ApplyDeprecation();
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif

const TSet<int32>& PCGExPointFilter::FRandomRatioFilter::GetCollectionPicks(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection)
{
	{
		FReadScopeLock ReadScopeLock(CollectionLock);
		if (bColPicksBuilt) { return CollectionPicks; }
	}

	{
		FWriteScopeLock WriteScopeLock(CollectionLock);
		bColPicksBuilt = true;
		TypedFilterFactory->Config.Random.GetPicks(IO->GetContext(), IO->GetIn(), ParentCollection->Num(), CollectionPicks);
	}

	return CollectionPicks;
}

bool PCGExPointFilter::FRandomRatioFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!IFilter::Init(InContext, InPointDataFacade)) { return false; }
	if (!bWillBeUsedWithCollections) { TypedFilterFactory->Config.Random.GetPicks(InContext, InPointDataFacade->GetIn(), InPointDataFacade->GetNum(), PointPicks); }
	return true;
}

bool PCGExPointFilter::FRandomRatioFilter::Test(const int32 PointIndex) const
{
	return TypedFilterFactory->Config.bInvertResult ? !PointPicks.Contains(PointIndex) : PointPicks.Contains(PointIndex);
}

bool PCGExPointFilter::FRandomRatioFilter::Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const
{
	const TSet<int32>& ColPicks = const_cast<FRandomRatioFilter*>(this)->GetCollectionPicks(IO, ParentCollection);
	return TypedFilterFactory->Config.bInvertResult ? !ColPicks.Contains(IO->IOIndex) : ColPicks.Contains(IO->IOIndex);
}

PCGEX_CREATE_FILTER_FACTORY(RandomRatio)

#if WITH_EDITOR
FString UPCGExRandomRatioFilterProviderSettings::GetDisplayName() const
{
	return TEXT("Random Ratio");
}
#endif


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
