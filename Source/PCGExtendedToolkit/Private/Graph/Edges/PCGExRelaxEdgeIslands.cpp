// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRelaxEdgeIslands.h"

#include "Graph/Edges/Relaxing/PCGExEdgeRelaxingOperation.h"
#include "Graph/Edges/Relaxing/PCGExForceDirectedRelaxing.h"

#define LOCTEXT_NAMESPACE "PCGExRelaxEdgeIslands"
#define PCGEX_NAMESPACE RelaxEdgeIslands

UPCGExRelaxEdgeIslandsSettings::UPCGExRelaxEdgeIslandsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PCGEX_DEFAULT_OPERATION(Relaxing, UPCGExForceDirectedRelaxing)
}

PCGExData::EInit UPCGExRelaxEdgeIslandsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGElementPtr UPCGExRelaxEdgeIslandsSettings::CreateElement() const { return MakeShared<FPCGExRelaxEdgeIslandsElement>(); }

FPCGExRelaxEdgeIslandsContext::~FPCGExRelaxEdgeIslandsContext()
{
	PCGEX_TERMINATE_ASYNC

	PrimaryBuffer.Empty();
	SecondaryBuffer.Empty();

	InfluenceGetter.Cleanup();
}

PCGEX_INITIALIZE_CONTEXT(RelaxEdgeIslands)

bool FPCGExRelaxEdgeIslandsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RelaxEdgeIslands)

	Context->Iterations = FMath::Max(Settings->Iterations, 1);
	PCGEX_FWD(bUseLocalInfluence)

	PCGEX_BIND_OPERATION(Relaxing, UPCGExForceDirectedRelaxing)

	Context->InfluenceGetter.Capture(Settings->LocalInfluence);
	Context->Relaxing->DefaultInfluence = Settings->Influence;

	return true;
}

bool FPCGExRelaxEdgeIslandsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelaxEdgeIslandsElement::Execute);

	PCGEX_CONTEXT(RelaxEdgeIslands)

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

			Context->Relaxing->Write(*Context->CurrentIO, Context->InfluenceGetter);
		}

		if (!Context->AdvanceAndBindPointsIO()) { Context->Done(); }
		else
		{
			if (!Context->BoundEdges->IsValid())
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
			Context->Relaxing->PrepareForMesh(*Context->CurrentEdges, Context->CurrentMesh);
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
			const PCGExMesh::FVertex& Vtx = Context->CurrentMesh->Vertices[VertexIndex];
			Context->Relaxing->ProcessVertex(Vtx);
		};

		while (Context->CurrentIteration != Context->Iterations)
		{
			if (Context->ProcessCurrentMesh(Initialize, ProcessVertex)) { Context->CurrentIteration++; }
			return false;
		}

		Context->SetState(PCGExGraph::State_ReadyForNextEdges);
	}

	if (Context->IsDone()) { Context->OutputPointsAndEdges(); }

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
