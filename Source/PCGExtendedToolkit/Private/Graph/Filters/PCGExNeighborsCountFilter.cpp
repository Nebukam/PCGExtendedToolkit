// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExNeighborsCountFilter.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeNeighborsCountFilter"
#define PCGEX_NAMESPACE NodeNeighborsCountFilter

PCGExDataFilter::TFilter* UPCGExNeighborsCountFilterFactory::CreateFilter() const
{
	return new PCGExNodeNeighborsCount::TNeighborsCountFilter(this);
}

namespace PCGExNodeNeighborsCount
{
	PCGExDataFilter::EType TNeighborsCountFilter::GetFilterType() const { return PCGExDataFilter::EType::Cluster; }

	void TNeighborsCountFilter::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		if (TypedFilterFactory->CompareAgainst == EPCGExOperandType::Attribute)
		{
			LocalCount = new PCGEx::FLocalSingleFieldGetter();
			LocalCount->Capture(TypedFilterFactory->LocalCount);
			LocalCount->Grab(*PointIO, false);

			bValid = LocalCount->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid LocalCount attribute: {0}."), FText::FromName(TypedFilterFactory->LocalCount.GetName())));
				PCGEX_DELETE(LocalCount)
			}
		}
	}

	void TNeighborsCountFilter::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
	}

	bool TNeighborsCountFilter::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
		const double A = Node.AdjacentNodes.Num();
		const double B = LocalCount ? LocalCount->Values[Node.PointIndex] : TypedFilterFactory->Count;
		return PCGExCompare::Compare(TypedFilterFactory->Comparison, A, B, TypedFilterFactory->Tolerance);
	}
}

PCGEX_CREATE_FILTER_FACTORY(NeighborsCount)

#if WITH_EDITOR
FString UPCGExNeighborsCountFilterProviderSettings::GetDisplayName() const
{
	FString DisplayName = "Neighbors Count" + PCGExCompare::ToString(Descriptor.Comparison);

	if (Descriptor.CompareAgainst == EPCGExOperandType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Descriptor.Count); }
	else { DisplayName += Descriptor.LocalCount.GetName().ToString(); }

	return DisplayName;
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
