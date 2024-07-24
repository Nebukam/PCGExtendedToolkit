// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExDrawEdges.h"

#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE DrawEdges

PCGExData::EInit UPCGExDrawEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExDrawEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }


PCGEX_INITIALIZE_ELEMENT(DrawEdges)

FPCGExDrawEdgesContext::~FPCGExDrawEdgesContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExDrawEdgesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

#if WITH_EDITOR

	PCGEX_CONTEXT_AND_SETTINGS(DrawEdges)

	if (!Settings->bPCGExDebug) { return false; }

#endif

	return true;
}

bool FPCGExDrawEdgesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDrawEdgesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DrawEdges)

#if WITH_EDITOR


	if (Context->IsSetup())
	{
		if (!Boot(Context))
		{
			DisabledPassThroughData(Context);
			return true;
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);

		Context->MaxLerp = static_cast<double>(Context->MainEdges->Num());

		return false;
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		while (Context->AdvanceEdges(true))
		{
			if (!Context->CurrentCluster)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("A cluster is corrupted."));
				continue;
			}

			double L = Context->CurrentLerp / Context->MaxLerp;
			FColor Col = Settings->bLerpColor ?
				             PCGExMath::Lerp(Settings->Color, Settings->SecondaryColor, L) :
				             Settings->Color;

			const TMap<int32, int32>& NodeIndexLookupRef = *Context->CurrentCluster->NodeIndexLookup;

			for (const PCGExGraph::FIndexedEdge& Edge : (*Context->CurrentCluster->Edges))
			{
				if (!Edge.bValid) { continue; }
				FVector Start = Context->CurrentCluster->GetPos(NodeIndexLookupRef[Edge.Start]);
				FVector End = Context->CurrentCluster->GetPos(NodeIndexLookupRef[Edge.End]);
				DrawDebugLine(Context->World, Start, End, Col, true, -1, Settings->DepthPriority, Settings->Thickness);
			}

			Context->CurrentLerp++;
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		DisabledPassThroughData(Context);
	}

	return Context->IsDone();

#else

	DisabledPassThroughData(Context);
	return true;
	
#endif
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
