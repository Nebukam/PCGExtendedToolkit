// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDeleteCustomGraph.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteCustomGraph"
#define PCGEX_NAMESPACE DeleteCustomGraph

int32 UPCGExDeleteCustomGraphSettings::GetPreferredChunkSize() const { return 32; }

TArray<FPCGPinProperties> UPCGExDeleteCustomGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

PCGExData::EInit UPCGExDeleteCustomGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(DeleteCustomGraph)

bool FPCGExDeleteCustomGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteCustomGraphElement::Execute);

	PCGEX_CONTEXT(DeleteCustomGraph)

	if (!Boot(Context)) { return true; }

	Context->MainPoints->ForEach(
		[&](const PCGExData::FPointIO& PointIO, const int32)
		{
			auto DeleteSockets = [&](const UPCGExGraphParamsData* Params, int32)
			{
				const UPCGPointData* OutData = PointIO.GetOut();
				for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
				{
					Socket.DeleteFrom(OutData);
				}
				OutData->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
			};
			Context->Graphs.ForEach(Context, DeleteSockets);
		});

	Context->OutputPoints();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
