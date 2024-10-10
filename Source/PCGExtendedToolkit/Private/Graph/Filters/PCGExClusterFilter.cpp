// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExClusterFilter.h"


#include "Graph/PCGExCluster.h"

namespace PCGExClusterFilter
{
	PCGExFilters::EType FFilter::GetFilterType() const { return PCGExFilters::EType::Node; }

	bool FFilter::Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade)
	{
		if (!bInitForCluster)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Using a Cluster filter without cluster data"));
			return false;
		}
		return PCGExPointFilter::FFilter::Init(InContext, InPointDataFacade);
	}

	bool FFilter::Init(const FPCGContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		bInitForCluster = true;
		Cluster = InCluster;
		EdgeDataFacade = InEdgeDataFacade;
		if (!PCGExPointFilter::FFilter::Init(InContext, InPointDataFacade)) { return false; }
		return true;
	}

	void FFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}

	bool TNodeFilter::Test(const int32 Index) const { return FFilter::Test(*(Cluster->Nodes->GetData() + Index)); }
	bool TNodeFilter::Test(const PCGExCluster::FNode& Node) const { return FFilter::Test(Node); }
	bool TNodeFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const PCGEX_NOT_IMPLEMENTED_RET(TVtxFilter::Test(const PCGExGraph::FIndexedEdge& Edge), false)

	bool TEdgeFilter::Test(const int32 Index) const { return FFilter::Test(*(Cluster->Edges->GetData() + Index)); }
	bool TEdgeFilter::Test(const PCGExCluster::FNode& Node) const PCGEX_NOT_IMPLEMENTED_RET(TEdgeFilter::Test(const PCGExCluster::FNode& Node), false)
	bool TEdgeFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const { return FFilter::Test(Edge); }

	FManager::FManager(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: PCGExPointFilter::FManager(InPointDataFacade), Cluster(InCluster), EdgeDataFacade(InEdgeDataFacade)
	{
	}

	bool FManager::InitFilter(const FPCGContext* InContext, const TSharedPtr<PCGExPointFilter::FFilter>& Filter)
	{
		if (Filter->GetFilterType() == PCGExFilters::EType::Point) { return Filter->Init(InContext, bUseEdgeAsPrimary ? EdgeDataFacade : PointDataFacade); }
		if (PCGExFactories::ClusterSpecificFilters.Contains(Filter->Factory->GetFactoryType()))
		{
			const TSharedPtr<FFilter> ClusterFilter = StaticCastSharedPtr<FFilter>(Filter);
			if (!ClusterFilter->Init(InContext, Cluster, PointDataFacade, EdgeDataFacade)) { return false; }
			return true;
		}
		return false;
	}

	void FManager::InitCache()
	{
		const int32 NumResults = Cluster->Nodes->Num();
		Results.Init(false, NumResults);
	}
}
