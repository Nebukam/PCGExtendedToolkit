// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Edges/PCGExEdgeEndpointsCompareStrFilter.h"


#include "Data/PCGExDataPreloader.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeEndpointsCompareStrFilter"
#define PCGEX_NAMESPACE EdgeEndpointsCompareStrFilter

void UPCGExEdgeEndpointsCompareStrFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	FacadePreloader.Register<FString>(InContext, Config.Attribute);
}

bool UPCGExEdgeEndpointsCompareStrFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(Config.Attribute, Consumable)

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExEdgeEndpointsCompareStrFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeEndpointsCompareStr::FNeighborsCountFilter>(this);
}

namespace PCGExEdgeEndpointsCompareStr
{
	bool FNeighborsCountFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!IFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		StringBuffer = InPointDataFacade->GetBroadcaster<FString>(TypedFilterFactory->Config.Attribute);
		if (!StringBuffer)
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Comparison Attribute ({0}) is not valid."), FText::FromString(PCGEx::GetSelectorDisplayName(TypedFilterFactory->Config.Attribute))));
			return false;
		}

		return true;
	}

	bool FNeighborsCountFilter::Test(const PCGExGraph::FEdge& Edge) const
	{
		const bool bResult = PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, StringBuffer->Read(Edge.Start), StringBuffer->Read(Edge.End));
		return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
	}

	FNeighborsCountFilter::~FNeighborsCountFilter()
	{
		TypedFilterFactory = nullptr;
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeEndpointsCompareStr)

#if WITH_EDITOR
FString UPCGExEdgeEndpointsCompareStrFilterProviderSettings::GetDisplayName() const
{
	return PCGEx::GetSelectorDisplayName(Config.Attribute) + PCGExCompare::ToString(Config.Comparison);
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
