// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Filters/PCGExNodeAdjacencyFilter.h"

#include "PCGPin.h"
#include "Graph/PCGExGraph.h"

#define LOCTEXT_NAMESPACE "PCGExNodeAdjacencyFilter"
#define PCGEX_NAMESPACE NodeAdjacencyFilter

#if WITH_EDITOR
FString FPCGExAdjacencyFilterDescriptor::GetDisplayName() const
{
	FString DisplayName = OperandA.GetName().ToString();

	switch (Comparison)
	{
	case EPCGExComparison::StrictlyEqual:
		DisplayName += " == ";
		break;
	case EPCGExComparison::StrictlyNotEqual:
		DisplayName += " != ";
		break;
	case EPCGExComparison::EqualOrGreater:
		DisplayName += " >= ";
		break;
	case EPCGExComparison::EqualOrSmaller:
		DisplayName += " <= ";
		break;
	case EPCGExComparison::StrictlyGreater:
		DisplayName += " > ";
		break;
	case EPCGExComparison::StrictlySmaller:
		DisplayName += " < ";
		break;
	case EPCGExComparison::NearlyEqual:
		DisplayName += " ~= ";
		break;
	case EPCGExComparison::NearlyNotEqual:
		DisplayName += " !~= ";
		break;
	default: DisplayName += " ?? ";
	}

	DisplayName += OperandB.GetName().ToString();
	DisplayName += TEXT(" (");

	switch (Mode)
	{
	case EPCGExAdjacencyTestMode::All:
		DisplayName += TEXT("All");
		break;
	case EPCGExAdjacencyTestMode::Some:
		DisplayName += TEXT("Some");
		break;
	default: ;
	}

	DisplayName += TEXT(")");
	return DisplayName;
}
#endif


PCGExDataFilter::TFilterHandler* UPCGExAdjacencyFilterDefinition::CreateHandler() const
{
	return new PCGExNodeAdjacency::TAdjacencyFilterHandler(this);
}

void UPCGExAdjacencyFilterDefinition::BeginDestroy()
{
	Super::BeginDestroy();
}

namespace PCGExNodeAdjacency
{
	void TAdjacencyFilterHandler::Capture(const PCGExData::FPointIO* PointIO)
	{
		TClusterFilterHandler::Capture(PointIO);
	}

	void TAdjacencyFilterHandler::CaptureEdges(const PCGExData::FPointIO* EdgeIO)
	{
		TClusterFilterHandler::CaptureEdges(EdgeIO);
	}

	bool TAdjacencyFilterHandler::Test(const int32 PointIndex) const
	{
		return TClusterFilterHandler::Test(PointIndex);
	}
}

FPCGElementPtr UPCGExNodeAdjacencyFilterSettings::CreateElement() const { return MakeShared<FPCGExNodeAdjacencyFilterElement>(); }

#if WITH_EDITOR
void UPCGExNodeAdjacencyFilterSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

bool FPCGExNodeAdjacencyFilterElement::ExecuteInternal(
	FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNodeAdjacencyFilterElement::Execute);

	PCGEX_SETTINGS(NodeAdjacencyFilter)

	UPCGExAdjacencyFilterDefinition* OutTest = NewObject<UPCGExAdjacencyFilterDefinition>();
	OutTest->ApplyDescriptor(Settings->Descriptor);

	FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
	Output.Data = OutTest;
	Output.Pin = PCGExGraph::OutputSocketStateLabel;

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
