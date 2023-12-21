// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDeleteGraph.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteGraph"
#define PCGEX_NAMESPACE DeleteGraph

int32 UPCGExDeleteGraphSettings::GetPreferredChunkSize() const { return 32; }

TArray<FPCGPinProperties> UPCGExDeleteGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

FPCGElementPtr UPCGExDeleteGraphSettings::CreateElement() const { return MakeShared<FPCGExDeleteGraphElement>(); }

PCGExData::EInit UPCGExDeleteGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_CONTEXT(DeleteGraph)

bool FPCGExDeleteGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteGraphElement::Execute);

	PCGEX_CONTEXT(DeleteGraph)

	if (!Boot(Context)) { return true; }

	Context->MainPoints->ForEach(
		[&](PCGExData::FPointIO& PointIO, const int32)
		{
			auto DeleteSockets = [&](const UPCGExGraphParamsData* Params, int32)
			{
				UPCGPointData* OutData = PointIO.GetOut();
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
