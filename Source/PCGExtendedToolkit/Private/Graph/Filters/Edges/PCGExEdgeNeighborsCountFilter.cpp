// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Edges/PCGExEdgeNeighborsCountFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeNeighborsCountFilter"
#define PCGEX_NAMESPACE EdgeNeighborsCountFilter

TSharedPtr<PCGExPointFilter::FFilter> UPCGExEdgeNeighborsCountFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeNeighborsCount::FNeighborsCountFilter>(this);
}

namespace PCGExEdgeNeighborsCount
{
	bool FNeighborsCountFilter::Init(const FPCGContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!FFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		if (TypedFilterFactory->Config.ThresholdSource == EPCGExFetchType::Attribute)
		{
			ThresholdBuffer = PointDataFacade->GetScopedBroadcaster<int32>(TypedFilterFactory->Config.ThresholdAttribute);
			if (!ThresholdBuffer)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Threshold Attribute ({0}) is not valid."), FText::FromString(TypedFilterFactory->Config.ThresholdAttribute.GetName().ToString())));
			}
		}

		return true;
	}

	bool FNeighborsCountFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const
	{
		const PCGExCluster::FNode* From = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.Start];
		const PCGExCluster::FNode* To = Cluster->Nodes->GetData() + (*Cluster->NodeIndexLookup)[Edge.End];

		// TODO : Make these lambdas

		const int32 Threshold = ThresholdBuffer ? ThresholdBuffer->Read(Edge.PointIndex) : TypedFilterFactory->Config.ThresholdConstant;
		const EPCGExComparison Comparison = TypedFilterFactory->Config.Comparison;
		const EPCGExRefineEdgeThresholdMode Mode = TypedFilterFactory->Config.Mode;
		const double Tolerance = TypedFilterFactory->Config.Tolerance;
		bool bResult = false;

		if (Mode == EPCGExRefineEdgeThresholdMode::Both)
		{
			bResult = PCGExCompare::Compare(Comparison, From->Adjacency.Num(), Threshold) && PCGExCompare::Compare(Comparison, To->Adjacency.Num(), Threshold, Tolerance);
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Any)
		{
			bResult = PCGExCompare::Compare(Comparison, From->Adjacency.Num(), Threshold) || PCGExCompare::Compare(Comparison, To->Adjacency.Num(), Threshold, Tolerance);
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Sum)
		{
			bResult = PCGExCompare::Compare(Comparison, (From->Adjacency.Num() + To->Adjacency.Num()), Threshold, Tolerance);
		}

		return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeNeighborsCount)

#if WITH_EDITOR
FString UPCGExEdgeNeighborsCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Neighbors Count (";

	switch (Config.Mode)
	{
	case EPCGExRefineEdgeThresholdMode::Sum:
		DisplayName += "Sum";
		break;
	case EPCGExRefineEdgeThresholdMode::Any:
		DisplayName += "Any";
		break;
	case EPCGExRefineEdgeThresholdMode::Both:
		DisplayName += "Both";
		break;
	}

	DisplayName += ")" + PCGExCompare::ToString(Config.Comparison);
	if (Config.ThresholdSource == EPCGExFetchType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.ThresholdConstant); }
	else { DisplayName += Config.ThresholdAttribute.GetName().ToString(); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
