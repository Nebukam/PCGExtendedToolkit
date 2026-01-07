// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Filters/Edges/PCGExEdgeNeighborsCountFilter.h"


#include "Clusters/PCGExCluster.h"
#include "Containers/PCGExManagedObjects.h"
#include "Graphs/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExEdgeNeighborsCountFilter"
#define PCGEX_NAMESPACE EdgeNeighborsCountFilter

PCGEX_SETTING_VALUE_IMPL(FPCGExEdgeNeighborsCountFilterConfig, Threshold, int32, ThresholdInput, ThresholdAttribute, ThresholdConstant)

bool UPCGExEdgeNeighborsCountFilterFactory::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	if (!Super::RegisterConsumableAttributesWithData(InContext, InData)) { return false; }

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_CONDITIONAL(Config.ThresholdInput == EPCGExInputValueType::Attribute, Config.ThresholdAttribute, Consumable)

	return true;
}

TSharedPtr<PCGExPointFilter::IFilter> UPCGExEdgeNeighborsCountFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExEdgeNeighborsCount::FFilter>(this);
}

namespace PCGExEdgeNeighborsCount
{
	bool FFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExClusters::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!IFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		ThresholdBuffer = TypedFilterFactory->Config.GetValueSettingThreshold(PCGEX_QUIET_HANDLING);
		if (!ThresholdBuffer->Init(PointDataFacade)) { return false; }

		return true;
	}

	bool FFilter::Test(const PCGExGraphs::FEdge& Edge) const
	{
		const PCGExClusters::FNode* From = Cluster->GetEdgeStart(Edge);
		const PCGExClusters::FNode* To = Cluster->GetEdgeEnd(Edge);

		// TODO : Make these lambdas

		const int32 Threshold = ThresholdBuffer->Read(Edge.PointIndex);
		const EPCGExComparison Comparison = TypedFilterFactory->Config.Comparison;
		const EPCGExRefineEdgeThresholdMode Mode = TypedFilterFactory->Config.Mode;
		const double Tolerance = TypedFilterFactory->Config.Tolerance;
		bool bResult = false;

		if (Mode == EPCGExRefineEdgeThresholdMode::Both)
		{
			bResult = PCGExCompare::Compare(Comparison, From->Num(), Threshold) && PCGExCompare::Compare(Comparison, To->Num(), Threshold, Tolerance);
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Any)
		{
			bResult = PCGExCompare::Compare(Comparison, From->Num(), Threshold) || PCGExCompare::Compare(Comparison, To->Num(), Threshold, Tolerance);
		}
		else if (Mode == EPCGExRefineEdgeThresholdMode::Sum)
		{
			bResult = PCGExCompare::Compare(Comparison, (From->Num() + To->Num()), Threshold, Tolerance);
		}

		return TypedFilterFactory->Config.bInvert ? !bResult : bResult;
	}

	FFilter::~FFilter()
	{
		TypedFilterFactory = nullptr;
	}
}

PCGEX_CREATE_FILTER_FACTORY(EdgeNeighborsCount)

#if WITH_EDITOR
FString UPCGExEdgeNeighborsCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Neighbors Count (";

	switch (Config.Mode)
	{
	case EPCGExRefineEdgeThresholdMode::Sum: DisplayName += "Sum";
		break;
	case EPCGExRefineEdgeThresholdMode::Any: DisplayName += "Any";
		break;
	case EPCGExRefineEdgeThresholdMode::Both: DisplayName += "Both";
		break;
	}

	DisplayName += ")" + PCGExCompare::ToString(Config.Comparison);
	if (Config.ThresholdInput == EPCGExInputValueType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.ThresholdConstant); }
	else { DisplayName += PCGExMetaHelpers::GetSelectorDisplayName(Config.ThresholdAttribute); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
