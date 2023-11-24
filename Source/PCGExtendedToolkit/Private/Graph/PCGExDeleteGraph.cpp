// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExDeleteGraph.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteGraph"

int32 UPCGExDeleteGraphSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExDeleteGraphSettings::CreateElement() const
{
	return MakeShared<FPCGExDeleteGraphElement>();
}

PCGExIO::EInitMode UPCGExDeleteGraphSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

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

	Context->Points->ForEach(
		[&](UPCGExPointIO* PointIO, int32)
		{
			auto DeleteSockets = [&](const UPCGExGraphParamsData* Params, int32)
			{
				for (const PCGExGraph::FSocket& Socket : Params->GetSocketMapping()->Sockets)
				{
					//TODO: Remove individual socket attributes
					Socket.DeleteFrom(PointIO->Out);
				}
				PointIO->Out->Metadata->DeleteAttribute(Params->CachedIndexAttributeName);
			};
			Context->Params.ForEach(Context, DeleteSockets);
		});

	Context->OutputPointsAndParams();

	return true;
}

#undef LOCTEXT_NAMESPACE
