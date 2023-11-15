// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExDeleteRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExDeleteRelations"

int32 UPCGExDeleteRelationsSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExDeleteRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExDeleteRelationsElement>();
}

PCGEx::EIOInit UPCGExDeleteRelationsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGContext* FPCGExDeleteRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExDeleteRelationsContext* Context = new FPCGExDeleteRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExDeleteRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExDeleteRelationsContext* Context = static_cast<FPCGExDeleteRelationsContext*>(InContext);
	// ...
}

bool FPCGExDeleteRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDeleteRelationsElement::Execute);

	FPCGExDeleteRelationsContext* Context = static_cast<FPCGExDeleteRelationsContext*>(InContext);

	if (Context->Params.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
		return true;
	}

	if (Context->Points->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
		return true;
	}

	Context->Points->ForEach(
		[&Context](UPCGExPointIO* PointIO, int32)
		{
			auto DeleteSockets = [&PointIO](UPCGExRelationsParamsData* Params, int32)
			{
				for (const PCGExRelational::FSocket& Socket : Params->GetSocketMapping()->Sockets)
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
