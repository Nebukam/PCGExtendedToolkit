// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExClusterFilter.h"

#include "Graph/PCGExCluster.h"

namespace PCGExClusterFilter
{
	PCGExFilters::EType TFilter::GetFilterType() const { return PCGExFilters::EType::Node; }

	bool TFilter::Init(const FPCGContext* InContext, PCGExData::FPool* InPointDataCache)
	{
		return PCGExPointFilter::TFilter::Init(InContext, InPointDataCache);
	}

	bool TFilter::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FPool* InPointDataCache, PCGExData::FPool* InEdgeDataCache)
	{
		Cluster = InCluster;
		EdgeDataCache = InEdgeDataCache;
		if (!PCGExPointFilter::TFilter::Init(InContext, InPointDataCache)) { return false; }
		return true;
	}

	void TFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataCache->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}

	TManager::TManager(PCGExCluster::FCluster* InCluster, PCGExData::FPool* InPointDataCache, PCGExData::FPool* InEdgeDataCache)
		: PCGExPointFilter::TManager(InPointDataCache), Cluster(InCluster), EdgeDataCache(InEdgeDataCache)
	{
	}

	bool TManager::InitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* Filter)
	{
		if (Filter->GetFilterType() == PCGExFilters::EType::Point) { return PCGExPointFilter::TManager::InitFilter(InContext, Filter); }
		if (Filter->GetFilterType() == PCGExFilters::EType::Node)
		{
			TFilter* ClusterFilter = static_cast<TFilter*>(Filter);
			if (!ClusterFilter->Init(InContext, Cluster, PointDataCache, EdgeDataCache)) { return false; }
			return true;
		}
		return false;
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
