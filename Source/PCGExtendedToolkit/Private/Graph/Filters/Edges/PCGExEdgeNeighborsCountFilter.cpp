// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Edges/PCGExEdgeNeighborsCountFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeNeighborsCountFilter"
#define PCGEX_NAMESPACE EdgeNeighborsCountFilter

TSharedPtr<PCGExPointFilter::TFilter> UPCGExEdgeNeighborsCountFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeNeighborsCount::FNeighborsCountFilter>(this);
}


namespace PCGExEdgeNeighborsCount
{
	bool FNeighborsCountFilter::Init(const FPCGContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade, const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!TFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		if (TypedFilterFactory->Config.CompareAgainst == EPCGExFetchType::Attribute)
		{
			LocalCount = PointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.LocalCount);

			if (!LocalCount)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid LocalCount attribute: \"{0}\"."), FText::FromName(TypedFilterFactory->Config.LocalCount.GetName())));
				return false;
			}
		}

		return true;
	}

	bool FNeighborsCountFilter::Test(const PCGExCluster::FNode& Node) const
	{
		const double A = Node.Adjacency.Num();
		const double B = LocalCount ? LocalCount->Read(Node.PointIndex) : TypedFilterFactory->Config.Count;
		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeNeighborsCount)

#if WITH_EDITOR
FString UPCGExEdgeNeighborsCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Neighbors Count" + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExFetchType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.Count); }
	else { DisplayName += Config.LocalCount.GetName().ToString(); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
