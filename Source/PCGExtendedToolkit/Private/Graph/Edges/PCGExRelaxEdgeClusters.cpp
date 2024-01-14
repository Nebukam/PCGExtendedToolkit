// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRelaxEdgeClusters.h"

#include "Graph/Edges/Relaxing/PCGExEdgeRelaxingOperation.h"
#include "Graph/Edges/Relaxing/PCGExForceDirectedRelaxing.h"

#define LOCTEXT_NAMESPACE "PCGExRelaxEdgeClusters"
#define PCGEX_NAMESPACE RelaxEdgeClusters

UPCGExRelaxEdgeClustersSettings::UPCGExRelaxEdgeClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_OPERATION_DEFAULT(Relaxing, UPCGExForceDirectedRelaxing)
}

PCGExData::EInit UPCGExRelaxEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(RelaxEdgeClusters)

FPCGExRelaxEdgeClustersContext::~FPCGExRelaxEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PrimaryBuffer.Empty();
	SecondaryBuffer.Empty();

	InfluenceGetter.Cleanup();
}

bool FPCGExRelaxEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RelaxEdgeClusters)

	Context->Iterations = FMath::Max(Settings->Iterations, 1);
	PCGEX_FWD(bUseLocalInfluence)

	PCGEX_OPERATION_BIND(Relaxing, UPCGExForceDirectedRelaxing)

	Context->InfluenceGetter.Capture(Settings->LocalInfluence);
	Context->Relaxing->DefaultInfluence = Settings->Influence;

	return true;
}

bool FPCGExRelaxEdgeClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelaxEdgeClustersElement::Execute);

	PCGEX_CONTEXT(RelaxEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		Context->CurrentIteration = 0;

		if (Context->CurrentIO)
		{
			// Dump buffers
			if (Context->bUseLocalInfluence)
			{
				Context->InfluenceGetter.bEnabled = true;
				Context->InfluenceGetter.Bind(*Context->CurrentIO);
			}
			else { Context->InfluenceGetter.bEnabled = false; }

			Context->Relaxing->WriteActiveBuffer(*Context->CurrentIO, Context->InfluenceGetter);
		}

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
				const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
				const int32 NumPoints = InPoints.Num();
				Context->PrimaryBuffer.SetNumUninitialized(NumPoints);
				Context->SecondaryBuffer.SetNumUninitialized(NumPoints);

				for (int i = 0; i < NumPoints; i++)
				{
					Context->PrimaryBuffer[i] =
						Context->SecondaryBuffer[i] = InPoints[i].Transform.GetLocation();
				}

				Context->Relaxing->PrepareForPointIO(*Context->CurrentIO);

				Context->SetState(PCGExGraph::State_ReadyForNextEdges);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_ReadyForNextEdges))
	{
		if (!Context->AdvanceEdges()) { Context->SetState(PCGExMT::State_ReadyForNextPoints); }
		else
		{
			Context->Relaxing->PrepareForCluster(*Context->CurrentEdges, Context->CurrentCluster);
			Context->SetState(PCGExGraph::State_ProcessingEdges);
		}
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		auto Initialize = [&]()
		{
			Context->Relaxing->PrepareForIteration(Context->CurrentIteration, &Context->PrimaryBuffer, &Context->SecondaryBuffer);
		};

		auto ProcessVertex = [&](const int32 VertexIndex)
		{
			const PCGExCluster::FNode& Vtx = Context->CurrentCluster->Nodes[VertexIndex];
			Context->Relaxing->ProcessVertex(Vtx);
		};

		while (Context->CurrentIteration != Context->Iterations)
		{
			if (Context->ProcessCurrentCluster(Initialize, ProcessVertex)) { Context->CurrentIteration++; }
			return false;
		}

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
