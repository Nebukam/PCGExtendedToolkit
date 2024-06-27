// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointFilter.h"

PCGExFactories::EType UPCGExFilterFactoryBase::GetFactoryType() const
{
	return PCGExFactories::EType::FilterPoint;
}

PCGExPointFilter::TFilter* UPCGExFilterFactoryBase::CreateFilter() const
{
	return nullptr;
}


void UPCGExFilterFactoryBase::Init()
{
}

namespace PCGExPointFilter
{
	PCGExFilters::EType TFilter::GetFilterType() const { return PCGExFilters::EType::Point; }

	bool TFilter::Init(const FPCGContext* InContext, PCGExDataCaching::FPool* InPointDataCache)
	{
		PointDataCache = InPointDataCache;
		return true;
	}

	void TFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = PointDataCache->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}

	TManager::TManager(PCGExDataCaching::FPool* InPointDataCache)
		: PointDataCache(InPointDataCache)
	{
	}

	bool TManager::Init(const FPCGContext* InContext, const TArray<UPCGExFilterFactoryBase*>& InFactories)
	{
		for (const UPCGExFilterFactoryBase* Factory : InFactories)
		{
			TFilter* NewFilter = Factory->CreateFilter();
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			if (!InitFilter(InContext, NewFilter)) { delete NewFilter; }
		}

		return PostInit(InContext);
	}

	bool TManager::TestPoint(const int32 Index)
	{
		for (const TFilter* Handler : PointFilters) { if (!Handler->Test(Index)) { return false; } }
		return true;
	}

	bool TManager::InitFilter(const FPCGContext* InContext, TFilter* Filter)
	{
		if (!Filter->Init(InContext, PointDataCache)) { return false; }

		PointFilters.Add(Filter);
		return true;
	}

	bool TManager::PostInit(const FPCGContext* InContext)
	{
		bValid = !PointFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		PointFilters.Sort([&](const TFilter& A, const TFilter& B) { return A.Factory->Priority < B.Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < PointFilters.Num(); i++)
		{
			TFilter* Filter = PointFilters[i];
			Filter->FilterIndex = i;
			PostInitFilter(Filter);
		}

		if (bCacheResults) { InitCache(); }

		return true;
	}

	void TManager::PostInitFilter(TFilter* Filter)
	{
		Filter->PostInit();
	}

	void TManager::InitCache()
	{
		const int32 NumResults = PointDataCache->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}
}
