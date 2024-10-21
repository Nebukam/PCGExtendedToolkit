// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Edges/PCGExEdgeEndpointsCompareNumFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeEndpointsCompareNumFilter"
#define PCGEX_NAMESPACE EdgeEndpointsCompareNumFilter

void UPCGExEdgeEndpointsCompareNumFilterFactory::GatherRequiredVtxAttributes(FPCGExContext* InContext, PCGExData::FReadableBufferConfigList& ReadableBufferConfigList) const
{
	Super::GatherRequiredVtxAttributes(InContext, ReadableBufferConfigList);
	ReadableBufferConfigList.Register<double>(InContext, Config.Attribute);
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExEdgeEndpointsCompareNumFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeEndpointsCompareNum::FNeighborsCountFilter>(this);
}

namespace PCGExEdgeEndpointsCompareNum
{
	bool FNeighborsCountFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!FFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		NumericBuffer = InPointDataFacade->GetBroadcaster<double>(TypedFilterFactory->Config.Attribute);
		if (!NumericBuffer)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Comparison Attribute ({0}) is not valid."), FText::FromString(TypedFilterFactory->Config.Attribute.GetName().ToString())));
			return false;
		}

		return true;
	}

	bool FNeighborsCountFilter::Test(const PCGExGraph::FIndexedEdge& Edge) const
	{
		const bool bResult = PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, NumericBuffer->Read(Edge.Start), NumericBuffer->Read(Edge.End), TypedFilterFactory->Config.Tolerance);
		return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeEndpointsCompareNum)

#if WITH_EDITOR
FString UPCGExEdgeEndpointsCompareNumFilterProviderSettings::GetDisplayName() const
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
		if (Config.ThresholdSource == EPCGExInputValueType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.ThresholdConstant); }
		else { DisplayName += Config.ThresholdAttribute.GetName().ToString(); }
	*/
	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
