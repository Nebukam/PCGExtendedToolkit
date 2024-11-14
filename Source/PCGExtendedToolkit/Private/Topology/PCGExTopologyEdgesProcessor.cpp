// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopologyEdgesProcessor.h"

#include "Topology/PCGExTopology.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE TopologyProcessor

PCGExData::EIOInit UPCGExTopologyEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExTopologyEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExTopologyEdgesProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SupportsEdgeConstraints())
	{
		PCGEX_PIN_PARAMS(PCGExTopology::SourceEdgeConstrainsFiltersLabel, "Constrained edges filters.", Normal, {})
	}
	return PinProperties;
}

bool FPCGExTopologyEdgesProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(TopologyEdgesProcessor)

	GetInputFactories(Context, PCGExTopology::SourceEdgeConstrainsFiltersLabel, Context->EdgeConstraintsFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	return true;
}

namespace PCGExTopology
{
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
