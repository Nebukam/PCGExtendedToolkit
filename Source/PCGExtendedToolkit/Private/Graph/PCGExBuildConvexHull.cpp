// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull

PCGExData::EInit UPCGExBuildConvexHullSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildConvexHullContext::~FPCGExBuildConvexHullContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildConvexHullSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExBuildConvexHullSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull)

bool FPCGExBuildConvexHullElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)

	if (!Settings->bPrunePoints) { PCGEX_VALIDATE_NAME(Settings->HullAttributeName) }

	Context->GraphBuilderSettings.bPruneIsolatedPoints = Settings->bPrunePoints;

	return true;
}

bool FPCGExBuildConvexHullElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHullElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->GraphBuilder)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 3)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 4)."));
				return false;
			}

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6);
			Context->GetAsyncManager()->Start<FPCGExConvexHull3Task>(Context->CurrentIO->IOIndex, Context->CurrentIO, Context->GraphBuilder->Graph);

			//PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));

			Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		PCGEX_WAIT_ASYNC

		if (Context->GraphBuilder->Graph->Edges.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		PCGEX_WAIT_ASYNC

		if (Context->GraphBuilder->bCompiledSuccessfully)
		{
			if (Settings->bMarkHull && !Settings->bPrunePoints)
			{
				PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
				HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

				for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

				HullMarkPointWriter->Write();
				PCGEX_DELETE(HullMarkPointWriter)
			}

			Context->GraphBuilder->Write(Context);
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExConvexHull3Task::ExecuteTask()
{
	FPCGExBuildConvexHullContext* Context = static_cast<FPCGExBuildConvexHullContext*>(Manager->Context);
	PCGEX_SETTINGS(BuildConvexHull)

	PCGExGeo::TDelaunay3* Delaunay = new PCGExGeo::TDelaunay3();

	const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = Points.Num();

	TArray<FVector> Positions;
	Positions.SetNumUninitialized(NumPoints);
	for (int i = 0; i < NumPoints; i++) { Positions[i] = Points[i].Transform.GetLocation(); }

	const TArrayView<FVector> View = MakeArrayView(Positions);
	if (!Delaunay->Process(View, true))
	{
		PCGEX_DELETE(Delaunay)
		return false;
	}

	if (Settings->bPrunePoints)
	{
		PCGExGraph::FIndexedEdge E = PCGExGraph::FIndexedEdge{};
		for (const uint64 Edge : Delaunay->DelaunayEdges)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Edge, A, B);
			const bool bAIsOnHull = Delaunay->DelaunayHull.Contains(A);
			const bool bBIsOnHull = Delaunay->DelaunayHull.Contains(B);

			if (!bAIsOnHull || !bBIsOnHull)
			{
				if (!bAIsOnHull) { Context->GraphBuilder->Graph->Nodes[A].bValid = false; }
				if (!bBIsOnHull) { Context->GraphBuilder->Graph->Nodes[B].bValid = false; }
				continue;
			}

			Graph->InsertEdge(A, B, E);
		}
	}
	else
	{
		if (Settings->bMarkHull) { Context->HullIndices.Append(Delaunay->DelaunayHull); }
		Graph->InsertEdges(Delaunay->DelaunayEdges, -1);
	}


	PCGEX_DELETE(Delaunay)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
