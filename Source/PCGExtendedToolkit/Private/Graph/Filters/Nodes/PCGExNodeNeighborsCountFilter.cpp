// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/Nodes/PCGExNodeNeighborsCountFilter.h"


#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeNeighborsCountFilter"
#define PCGEX_NAMESPACE NodeNeighborsCountFilter

void UPCGExNodeNeighborsCountFilterFactory::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
	Super::RegisterBuffersDependencies(InContext, FacadePreloader);
	if (Config.CompareAgainst == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, Config.LocalCount); }
}

TSharedPtr<PCGExPointFilter::FFilter> UPCGExNodeNeighborsCountFilterFactory::CreateFilter() const
{
	return MakeShared<PCGExNodeNeighborsCount::FNeighborsCountFilter>(this);
}

namespace PCGExNodeNeighborsCount
{
	bool FNeighborsCountFilter::Init(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster>& InCluster, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
	{
		if (!FFilter::Init(InContext, InCluster, InPointDataFacade, InEdgeDataFacade)) { return false; }

		if (TypedFilterFactory->Config.CompareAgainst == EPCGExInputValueType::Attribute)
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
		const double A = Node.Num();
		const double B = LocalCount ? LocalCount->Read(Node.PointIndex) : TypedFilterFactory->Config.Count;
		return PCGExCompare::Compare(TypedFilterFactory->Config.Comparison, A, B, TypedFilterFactory->Config.Tolerance);
	}
}

PCGEX_CREATE_FILTER_FACTORY(NodeNeighborsCount)

#if WITH_EDITOR
FString UPCGExNodeNeighborsCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Neighbors Count" + PCGExCompare::ToString(Config.Comparison);

	if (Config.CompareAgainst == EPCGExInputValueType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Config.Count); }
	else { DisplayName += PCGEx::GetSelectorDisplayName(Config.LocalCount); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
