// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExClusterFilter.h"

#include "Graph/PCGExCluster.h"

namespace PCGExClusterFilter
{
	PCGExFilters::EType TFilter::GetFilterType() const { return PCGExFilters::EType::Node; }

	bool TFilter::Init(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
	{
		if (!bInitForCluster)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Using a Cluster filter without cluster data"));
			return false;
		}
		return PCGExPointFilter::TFilter::Init(InContext, InPointDataFacade);
	}

	bool TFilter::Init(const FPCGContext* InContext, PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade)
	{
		bInitForCluster = true;
		Cluster = InCluster;
		EdgeDataFacade = InEdgeDataFacade;
		if (!PCGExPointFilter::TFilter::Init(InContext, InPointDataFacade)) { return false; }
		return true;
	}

	void TFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataFacade->Source->GetNum();
		Results.SetNumUninitialized(NumResults);
		for (bool& Result : Results) { Result = false; }
	}

	TManager::TManager(PCGExCluster::FCluster* InCluster, PCGExData::FFacade* InPointDataFacade, PCGExData::FFacade* InEdgeDataFacade)
		: PCGExPointFilter::TManager(InPointDataFacade), Cluster(InCluster), EdgeDataCache(InEdgeDataFacade)
	{
	}

	bool TManager::InitFilter(const FPCGContext* InContext, PCGExPointFilter::TFilter* Filter)
	{
		if (Filter->GetFilterType() == PCGExFilters::EType::Point) { return PCGExPointFilter::TManager::InitFilter(InContext, Filter); }
		if (Filter->GetFilterType() == PCGExFilters::EType::Node)
		{
			TFilter* ClusterFilter = static_cast<TFilter*>(Filter);
			if (!ClusterFilter->Init(InContext, Cluster, PointDataFacade, EdgeDataCache)) { return false; }
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
