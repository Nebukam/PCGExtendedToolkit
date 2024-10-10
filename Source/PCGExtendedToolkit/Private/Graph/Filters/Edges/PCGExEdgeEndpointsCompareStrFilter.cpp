// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Edges/PCGExEdgeEndpointsCompareStrFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeEndpointsCompareStrFilter"
#define PCGEX_NAMESPACE EdgeEndpointsCompareStrFilter

TSharedPtr<PCGExPointFilter::FFilter> UPCGExEdgeEndpointsCompareStrFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeEndpointsCompareStr::FNeighborsCountFilter>(this);
}

namespace PCGExEdgeEndpointsCompareStr
{
	bool FNeighborsCountFilter::Init(const FPCGContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!FFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		StringBuffer = InPointDataFacade->GetBroadcaster<FString>(TypedFilterFactory->Config.Attribute);
		if (!StringBuffer)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Comparison Attribute ({0}) is not valid."), FText::FromString(TypedFilterFactory->Config.Attribute.GetName().ToString())));
			return false;
		}

		return true;
	}

	bool FNeighborsCountFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const
	{
		const bool bResult = PCGExCompare::Compare(TypedFilterFactory->Config.StringComparison, StringBuffer->Read(Edge.Start), StringBuffer->Read(Edge.End));
		return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeEndpointsCompareStr)

#if WITH_EDITOR
FString UPCGExEdgeEndpointsCompareStrFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Compare Endpoints"; //" (";
	/*
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
	*/
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
