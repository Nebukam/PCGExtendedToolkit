// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull2D

PCGExData::EInit UPCGExBuildConvexHull2DSettings::GetMainOutputInitMode() const { return bPrunePoints ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExBuildConvexHull2DContext::~FPCGExBuildConvexHull2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PathsIO)

	HullIndices.Empty();

	ProjectionSettings.Cleanup();
}

TArray<FPCGPinProperties> UPCGExBuildConvexHull2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Point data representing closed convex hull paths.", Required, {})
	return PinProperties;
}

FName UPCGExBuildConvexHull2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull2D)

bool FPCGExBuildConvexHull2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->GraphBuilderSettings.bPruneIsolatedPoints = Settings->bPrunePoints;

	Context->PathsIO = new PCGExData::FPointIOCollection();
	Context->PathsIO->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	PCGEX_FWD(ProjectionSettings)

	return true;
}

bool FPCGExBuildConvexHull2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHull2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

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
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->ProjectionSettings.Init(Context->CurrentIO);

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6);
			Context->GetAsyncManager()->Start<FPCGExConvexHull2Task>(Context->CurrentIO->IOIndex, Context->CurrentIO, Context->GraphBuilder->Graph);

			//PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));

			Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		PCGEX_WAIT_ASYNC

		if (Context->GraphBuilder->Graph->Edges.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results."));
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

			Context->BuildPath();
			Context->GraphBuilder->Write(Context);
		}
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints(Settings->bPrunePoints);
		Context->PathsIO->OutputTo(Context);
	}

	return Context->IsDone();
}

void FPCGExBuildConvexHull2DContext::BuildPath() const
{
	TSet<uint64> UniqueEdges;
	const TArray<PCGExGraph::FIndexedEdge>& Edges = GraphBuilder->Graph->Edges;

	const PCGExData::FPointIO& PathIO = PathsIO->Emplace_GetRef(*CurrentIO, PCGExData::EInit::NewOutput);

	TArray<FPCGPoint>& MutablePathPoints = PathIO.GetOut()->GetMutablePoints();
	TSet<int32> VisitedEdges;
	VisitedEdges.Reserve(Edges.Num());

	int32 CurrentIndex = -1;
	int32 FirstIndex = -1;

	while (MutablePathPoints.Num() != Edges.Num())
	{
		int32 EdgeIndex = -1;

		if (CurrentIndex == -1)
		{
			EdgeIndex = 0;
			FirstIndex = Edges[EdgeIndex].Start;
			MutablePathPoints.Add(CurrentIO->GetInPoint(FirstIndex));
			CurrentIndex = Edges[EdgeIndex].End;
		}
		else
		{
			for (int i = 0; i < Edges.Num(); i++)
			{
				EdgeIndex = i;
				if (VisitedEdges.Contains(EdgeIndex)) { continue; }

				const PCGExGraph::FUnsignedEdge& Edge = Edges[EdgeIndex];
				if (!Edge.Contains(CurrentIndex)) { continue; }

				CurrentIndex = Edge.Other(CurrentIndex);
				break;
			}
		}

		if (CurrentIndex == FirstIndex) { break; }

		VisitedEdges.Add(EdgeIndex);
		MutablePathPoints.Add(CurrentIO->GetInPoint(CurrentIndex));
	}

	VisitedEdges.Empty();
}

bool FPCGExConvexHull2Task::ExecuteTask()
{
	FPCGExBuildConvexHull2DContext* Context = static_cast<FPCGExBuildConvexHull2DContext*>(Manager->Context);
	PCGEX_SETTINGS(BuildConvexHull2D)

	PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();

	TArray<FVector> Positions;
	PCGExGeo::PointsToPositions(Context->CurrentIO->GetIn()->GetPoints(), Positions);

	const TArrayView<FVector> View = MakeArrayView(Positions);
	if (!Delaunay->Process(View, Context->ProjectionSettings))
	{
		PCGEX_DELETE(Delaunay)
		return false;
	}

	if (!Settings->bPrunePoints)
	{
		if (Settings->bMarkHull) { Context->HullIndices.Append(Delaunay->DelaunayHull); }
		Graph->InsertEdges(Delaunay->DelaunayEdges, -1);
	}

	PCGExGraph::FIndexedEdge E = PCGExGraph::FIndexedEdge{};

	for (const uint64 Edge : Delaunay->DelaunayEdges)
	{
		uint32 A;
		uint32 B;
		PCGEx::H64(Edge, A, B);
		if (!Delaunay->DelaunayHull.Contains(A) ||
			!Delaunay->DelaunayHull.Contains(B)) { continue; }
		Graph->InsertEdge(A, B, E);
	}


	PCGEX_DELETE(Delaunay)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
