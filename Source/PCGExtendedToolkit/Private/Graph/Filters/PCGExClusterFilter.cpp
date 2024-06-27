// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExClusterFilter.h"

#include "Graph/PCGExCluster.h"

namespace PCGExClusterFilter
{
	PCGExFilters::EType TFilter::GetFilterType() const { return PCGExFilters::EType::Node; }

	bool TFilter::Init(const FPCGContext* InContext, PCGExDataCaching::FPool* InPointDataCache)
	{
		return PCGExPointFilter::TFilter::Init(InContext, InPointDataCache);
	}

	bool TFilter::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExDataCaching::FPool* InPointDataCache, PCGExDataCaching::FPool* InEdgeDataCache)
	{
		EdgeDataCache = InEdgeDataCache;
		if (!PCGExPointFilter::TFilter::Init(InContext, InPointDataCache)) { return false; }
		Cluster = InCluster;
		return true;
	}

	void TFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataCache->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}

	bool TFilter::Test(const int32 Index) const
	{
		// Should never be called
		check(false)
		return true;
	} 

	TManager::TManager(PCGExCluster::FCluster* InCluster, PCGExDataCaching::FPool* InPointDataCache, PCGExDataCaching::FPool* InEdgeDataCache)
		: PCGExPointFilter::TManager(InPointDataCache), Cluster(InCluster), EdgeDataCache(InEdgeDataCache)
	{
	}

	bool TManager::TestNode(const PCGExCluster::FNode& Node)
	{
		if (!PCGExPointFilter::TManager::TestPoint(Node.PointIndex)) { return false; }
		for (const TFilter* Handler : NodeFilters) { if (!Handler->Test(Node)) { return false; } }
		return true;
	}

	bool TManager::InitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* Filter)
	{
		if (Filter->GetFilterType() == PCGExFilters::EType::Point) { return PCGExPointFilter::TManager::InitFilter(InContext, Filter); }

		if (Filter->GetFilterType() == PCGExFilters::EType::Node)
		{
			TFilter* ClusterFilter = static_cast<TFilter*>(Filter);
			if (!ClusterFilter->Init(InContext, Cluster, PointDataCache, EdgeDataCache)) { return false; }

			NodeFilters.Add(ClusterFilter);
			return true;
		}

		return false;
	}

	bool TManager::PostInit(const FPCGContext* InContext)
	{
		return PCGExPointFilter::TManager::PostInit(InContext);
	}

	void TManager::InitCache()
	{
		const int32 NumResults = Cluster->Nodes->Num();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}
}

PCGExFactories::EType UPCGExNodeFilterFactoryBase::GetFactoryType() const { return PCGExFactories::EType::FilterNode; }
PCGExFactories::EType UPCGExEdgeFilterFactoryBase::GetFactoryType() const { return PCGExFactories::EType::FilterEdge; }
