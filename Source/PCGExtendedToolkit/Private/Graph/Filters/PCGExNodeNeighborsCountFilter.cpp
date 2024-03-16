// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExNodeNeighborsCountFilter.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeNeighborsCountFilter"
#define PCGEX_NAMESPACE NodeNeighborsCountFilter

#if WITH_EDITOR
FString FPCGExNeighborsCountFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = "Neighbors Count" + PCGExCompare::ToString(Comparison);

	if (CompareAgainst == EPCGExOperandType::Constant) { DisplayName += FString::Printf(TEXT("%d"), Count); }
	else { DisplayName += LocalCount.GetName().ToString(); }

	return DisplayName;
}
#endif


PCGExDataFilter::TFilterHandler* UPCGExNeighborsCountFilterDefinition::CreateHandler() const
{
	return new PCGExNodeNeighborsCount::TNeighborsCountFilterHandler(this);
}

void UPCGExNeighborsCountFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExNodeNeighborsCount
{
	void TNeighborsCountFilterHandler::Capture(const FPCGContext* InContext, const PCGExData::FPointIO* PointIO)
	{
		if (NeighborsCountFilter->CompareAgainst == EPCGExOperandType::Attribute)
		{
			LocalCount = new PCGEx::FLocalSingleFieldGetter();
			LocalCount->Capture(NeighborsCountFilter->LocalCount);
			LocalCount->Grab(*PointIO, false);

			bValid = LocalCount->IsUsable(PointIO->GetNum());

			if (!bValid)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid LocalCount attribute: {0}."), FText::FromName(NeighborsCountFilter->LocalCount.GetName())));
				PCGEX_DELETE(LocalCount)
			}
		}
	}

	void TNeighborsCountFilterHandler::CaptureEdges(const FPCGContext* InContext, const PCGExData::FPointIO* EdgeIO)
	{
	}

	bool TNeighborsCountFilterHandler::Test(const int32 PointIndex) const
	{
		const PCGExCluster::FNode& Node = CapturedCluster->Nodes[PointIndex];
		const double A = Node.AdjacentNodes.Num();
		const double B = LocalCount ? LocalCount->Values[Node.PointIndex] : NeighborsCountFilter->Count;
		return PCGExCompare::Compare(NeighborsCountFilter->Comparison, A, B, NeighborsCountFilter->Tolerance);
	}
}

FPCGElementPtr UPCGExNodeNeighborsCountFilterSettings::CreateElement() const { return MakeShared<FPCGExNodeNeighborsCountFilterElement>(); }

#if WITH_EDITOR
void UPCGExNodeNeighborsCountFilterSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExNodeNeighborsCountFilterElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNodeNeighborsCountFilterElement::Execute);

	PCGEX_SETTINGS(NodeNeighborsCountFilter)

	UPCGExNeighborsCountFilterDefinition* OutTest = NewObject<UPCGExNeighborsCountFilterDefinition>();
	OutTest->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutTest;
	Output.Pin = PCGExGraph::OutputSocketStateLabel;

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
