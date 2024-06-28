// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointFilter.h"

#include "Graph/PCGExCluster.h"

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

	bool TFilter::Init(const FPCGContext* InContext, PCGExData::FPool* InPointDataCache)
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

	bool TFilter::Test(const int32 Index) const { return DefaultResult; }
	bool TFilter::Test(const PCGExCluster::FNode& Node) const { return Test(Node.PointIndex); }
	bool TFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const { return Test(Edge.PointIndex); }

	TManager::TManager(PCGExData::FPool* InPointDataCache)
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
			ManagedFilters.Add(NewFilter);
		}

		return PostInit(InContext);
	}

	bool TManager::Test(const int32 Index)
	{
		for (const TFilter* Handler : ManagedFilters) { if (!Handler->Test(Index)) { return false; } }
		return true;
	}

	bool TManager::Test(const PCGExCluster::FNode& Node)
	{
		for (const TFilter* Handler : ManagedFilters) { if (!Handler->Test(Node)) { return false; } }
		return true;
	}

	bool TManager::Test(const PCGExGraph::FIndexedEdge& Edge)
	{
		for (const TFilter* Handler : ManagedFilters) { if (!Handler->Test(Edge)) { return false; } }
		return true;
	}

	bool TManager::InitFilter(const FPCGContext* InContext, TFilter* Filter)
	{
		return Filter->Init(InContext, PointDataCache);
	}

	bool TManager::PostInit(const FPCGContext* InContext)
	{
		bValid = !ManagedFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		ManagedFilters.Sort([&](const TFilter& A, const TFilter& B) { return A.Factory->Priority < B.Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < ManagedFilters.Num(); i++)
		{
			TFilter* Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitFilter(InContext, Filter);
		}

		if (bCacheResults) { InitCache(); }

		return true;
	}

	void TManager::PostInitFilter(const FPCGContext* InContext, TFilter* InFilter)
	{
		InFilter->PostInit();
	}

	void TManager::InitCache()
	{
		const int32 NumResults = PointDataCache->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}
}
