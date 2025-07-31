// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExClusterFilter.h"

#include "Graph/PCGExCluster.h"

namespace PCGExClusterFilter
{
	PCGExFilters::EType IFilter::GetFilterType() const { return PCGExFilters::EType::Node; }

	bool IFilter::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
	{
		if (!bInitForCluster)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Using a Cluster filter without cluster data"));
			return false;
		}
		return PCGExPointFilter::IFilter::Init(InContext, InPointDataFacade);
	}

	bool IFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		bInitForCluster = true;
		Cluster = InCluster;
		EdgeDataFacade = InEdgeDataFacade;
		if (!PCGExPointFilter::IFilter::Init(InContext, InPointDataFacade)) { return false; }
		return true;
	}

	void IFilter::PostInit()
	{
		if (!bCacheResults) { return; }
		const int32 NumResults = GetFilterType() == PCGExFilters::EType::Node ? Cluster->Nodes->Num() : EdgeDataFacade->Source->GetNum();
		Results.Init(false, NumResults);
	}

	bool IVtxFilter::Test(const int32 Index) const { return IFilter::Test(*Cluster->GetNode(Index)); }
	bool IVtxFilter::Test(const PCGExCluster::FNode& Node) const { return IFilter::Test(Node); }

	bool IVtxFilter::Test(const PCGExGraph::FEdge& Edge) const
	PCGEX_NOT_IMPLEMENTED_RET(TVtxFilter::Test(const PCGExGraph::FIndexedEdge& Edge), false)

	bool IEdgeFilter::Test(const int32 Index) const { return IFilter::Test(*Cluster->GetEdge(Index)); }

	bool IEdgeFilter::Test(const PCGExCluster::FNode& Node) const
	PCGEX_NOT_IMPLEMENTED_RET(TEdgeFilter::Test(const PCGExCluster::FNode& Node), false)

	bool IEdgeFilter::Test(const PCGExGraph::FEdge& Edge) const { return IFilter::Test(Edge); }

	FManager::FManager(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
		: PCGExPointFilter::FManager(InPointDataFacade), Cluster(InCluster), EdgeDataFacade(InEdgeDataFacade)
	{
	}

	bool FManager::InitFilter(FPCGExContext* InContext, const TSharedPtr<PCGExPointFilter::IFilter>& Filter)
	{
		if (PCGExFactories::SupportsClusterFilters.Contains(Filter->Factory->GetFactoryType()))
		{
			const TSharedPtr<IFilter> ClusterFilter = StaticCastSharedPtr<IFilter>(Filter);
			return ClusterFilter->Init(InContext, Cluster, PointDataFacade, EdgeDataFacade);
		}

		return Filter->Init(InContext, bUseEdgeAsPrimary ? EdgeDataFacade : PointDataFacade);
	}

	void FManager::InitCache()
	{
		const int32 NumResults = Cluster->Nodes->Num();
		Results.Init(false, NumResults);
	}
}
