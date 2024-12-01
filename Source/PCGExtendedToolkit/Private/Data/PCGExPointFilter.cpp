// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPointFilter.h"


#include "Graph/PCGExCluster.h"

TSharedPtr<PCGExPointFilter::FFilter> UPCGExFilterFactoryBase::CreateFilter() const
{
	return nullptr;
}


bool UPCGExFilterFactoryBase::Init(FPCGExContext* InContext)
{
	return true;
}

namespace PCGExPointFilter
{
	bool FFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		PointDataFacade = InPointDataFacade;
		return true;
	}

	void FFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = PointDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}

	bool FFilter::Test(const int32 Index) const PCGEX_NOT_IMPLEMENTED_RET(FFilter::Test(const int32 Index), false)
	bool FFilter::Test(const PCGExCluster::FNode& Node) const { return Test(Node.PointIndex); }
	bool FFilter::Test(const PCGExGraph::FEdge& Edge) const { return Test(Edge.PointIndex); }

	bool FSimpleFilter::Test(const int32 Index) const PCGEX_NOT_IMPLEMENTED_RET(TEdgeFilter::Test(const PCGExCluster::FNode& Node), false)
	bool FSimpleFilter::Test(const PCGExCluster::FNode& Node) const { return Test(Node.PointIndex); }
	bool FSimpleFilter::Test(const PCGExGraph::FEdge& Edge) const { return Test(Edge.PointIndex); }

	FManager::FManager(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
		: PointDataFacade(InPointDataFacade)
	{
	}

	bool FManager::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExFilterFactoryBase>>& InFactories)
	{
		for (const UPCGExFilterFactoryBase* Factory : InFactories)
		{
			TSharedPtr<FFilter> NewFilter = Factory->CreateFilter();
			NewFilter->bCacheResults = bCacheResultsPerFilter;
			NewFilter->bUseEdgeAsPrimary = bUseEdgeAsPrimary;
			if (!InitFilter(InContext, NewFilter)) { continue; }
			ManagedFilters.Add(NewFilter);
		}

		return PostInit(InContext);
	}

	bool FManager::Test(const int32 Index)
	{
		for (const TSharedPtr<FFilter>& Handler : ManagedFilters) { if (!Handler->Test(Index)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExCluster::FNode& Node)
	{
		for (const TSharedPtr<FFilter>& Handler : ManagedFilters) { if (!Handler->Test(Node)) { return false; } }
		return true;
	}

	bool FManager::Test(const PCGExGraph::FEdge& Edge)
	{
		for (const TSharedPtr<FFilter>& Handler : ManagedFilters) { if (!Handler->Test(Edge)) { return false; } }
		return true;
	}

	bool FManager::InitFilter(FPCGExContext* InContext, const TSharedPtr<FFilter>& Filter)
	{
		return Filter->Init(InContext, PointDataFacade);
	}

	bool FManager::PostInit(FPCGExContext* InContext)
	{
		bValid = !ManagedFilters.IsEmpty();

		if (!bValid) { return false; }

		// Sort mappings so higher priorities come last, as they have to potential to override values.
		ManagedFilters.Sort([](const TSharedPtr<FFilter>& A, const TSharedPtr<FFilter>& B) { return A->Factory->Priority < B->Factory->Priority; });

		// Update index & post-init
		for (int i = 0; i < ManagedFilters.Num(); i++)
		{
			TSharedPtr<FFilter> Filter = ManagedFilters[i];
			Filter->FilterIndex = i;
			PostInitFilter(InContext, Filter);
		}

		if (bCacheResults) { InitCache(); }

		return true;
	}

	void FManager::PostInitFilter(FPCGExContext* InContext, const TSharedPtr<FFilter>& InFilter)
	{
		InFilter->PostInit();
	}

	void FManager::InitCache()
	{
		const int32 NumResults = PointDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}
}
