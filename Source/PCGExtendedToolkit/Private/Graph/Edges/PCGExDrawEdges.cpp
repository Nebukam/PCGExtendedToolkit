// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExDrawEdges.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE DrawEdges

PCGExData::EIOInit UPCGExDrawEdgesSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }
PCGExData::EIOInit UPCGExDrawEdgesSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::None; }


PCGEX_INITIALIZE_ELEMENT(DrawEdges)

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

	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGEx::State_ReadyForNextPoints);
		Context->MaxLerp = static_cast<double>(Context->MainEdges->Num());
	}

	PCGEX_ON_STATE(PCGEx::State_ReadyForNextPoints)
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no bound edges."));
				Context->SetState(PCGEx::State_ReadyForNextPoints);
			}
			else
			{
				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	PCGEX_ON_STATE(PCGExGraph::State_ReadyForNextEdges)
	{
		const UWorld* World = Context->SourceComponent->GetWorld();

		/*
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
				DrawDebugLine(World, Start, End, Col, true, -1, Settings->DepthPriority, Settings->Thickness);
			}

			Context->CurrentLerp++;
		}
		*/

		Context->SetState(PCGEx::State_ReadyForNextPoints);
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
