// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDeleteGraph.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteGraph"

int32 UPCGExDeleteGraphSettings::GetPreferredChunkSize() const { return 32; }

TArray<FPCGPinProperties> UPCGExDeleteGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PinProperties.Pop();
	return PinProperties;
}

FPCGElementPtr UPCGExDeleteGraphSettings::CreateElement() const { return MakeShared<FPCGExDeleteGraphElement>(); }

PCGExData::EInit UPCGExDeleteGraphSettings::GetPointOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGContext* FPCGExDeleteGraphElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDeleteGraphContext* Context = new FPCGExDeleteGraphContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExDeleteGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteGraphElement::Execute);

	FPCGExDeleteGraphContext* Context = static_cast<FPCGExDeleteGraphContext*>(InContext);
	if (!Validate(Context)) { return true; }

	Context->MainPoints->ForEach(
		[&](PCGExData::FPointIO* PointIO, int32)
		{
			auto DeleteSockets = [&](const UPCGExGraphParamsData* Params, int32)
			{
				UPCGPointData* OutData = PointIO->GetOut();
				for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
				{
					//TODO: Remove individual socket attributes
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
