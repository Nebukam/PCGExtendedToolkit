// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Edges/PCGExEdgeLengthFilter.h"


#include "PCGExDetailsData.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeLengthFilter"
#define PCGEX_NAMESPACE EdgeLengthFilter

bool UPCGExEdgeLengthFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.ThresholdInput == EPCGExInputValueType::Attribute, Config.ThresholdAttribute, Consumable)

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExEdgeLengthFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeLength::FLengthFilter>(this);
}

namespace PCGExEdgeLength
{
	bool FLengthFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!IFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		Threshold = TypedFilterFactory->Config.GetValueSettingThreshold();
		if (!Threshold->Init(InContext, PointDataFacade)) { return false; }

		return true;
	}

	bool FLengthFilter::Test(const PCGExGraph::FEdge& Edge) const
	{
		return PCGExCompare::Compare(
			       TypedFilterFactory->Config.Comparison, Cluster->GetEdgeLength(Edge),
			       Threshold->Read(Edge.PointIndex), TypedFilterFactory->Config.Tolerance) ?
			       !TypedFilterFactory->Config.bInvert : TypedFilterFactory->Config.bInvert;
	}

	FLengthFilter::~FLengthFilter()
	{
		TypedFilterFactory = nullptr;
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeLength)

#if WITH_EDITOR
FString UPCGExEdgeLengthFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Edge Length ";
	DisplayName += PCGExCompare::ToString(Config.Comparison);
	if (Config.ThresholdInput == EPCGExInputValueType::Constant) { DisplayName += FString::Printf(TEXT("%f"), Config.ThresholdConstant); }
	else { DisplayName += PCGEx::GetSelectorDisplayName(Config.ThresholdAttribute); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
