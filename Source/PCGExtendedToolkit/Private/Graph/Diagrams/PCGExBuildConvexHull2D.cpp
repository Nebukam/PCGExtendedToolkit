// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Diagrams/PCGExBuildConvexHull2D.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull2D

PCGExData::EInit UPCGExBuildConvexHull2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

TArray<FPCGPinProperties> UPCGExBuildConvexHull2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Point data representing closed convex hull paths.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull2D)

bool FPCGExBuildConvexHull2DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	Context->PathsIO = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->PathsIO->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExBuildConvexHull2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHull2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConvexHull2D::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExConvexHull2D::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to build from."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();
	Context->PathsIO->StageOutputs();

	return Context->TryComplete();
}

void FPCGExBuildConvexHull2DContext::BuildPath(const PCGExGraph::FGraphBuilder* GraphBuilder) const
{
	TSet<uint64> UniqueEdges;
	const TArray<PCGExGraph::FIndexedEdge>& Edges = GraphBuilder->Graph->Edges;

	const TArray<FPCGPoint>& InPoints = GraphBuilder->NodeDataFacade->GetIn()->GetPoints();
	const TSharedPtr<PCGExData::FPointIO> PathIO = PathsIO->Emplace_GetRef(GraphBuilder->NodeDataFacade->GetIn(), PCGExData::EInit::NewOutput);

	TArray<FPCGPoint>& MutablePathPoints = PathIO->GetOut()->GetMutablePoints();
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
			MutablePathPoints.Add(InPoints[FirstIndex]);
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
		MutablePathPoints.Add(InPoints[CurrentIndex]);
	}

	VisitedEdges.Empty();
}

namespace PCGExConvexHull2D
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConvexHull2D::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		ProjectionDetails.Init(ExecutionContext, PointDataFacade);

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->Source->GetIn()->GetPoints(), ActivePositions);

		Delaunay = MakeUnique<PCGExGeo::TDelaunay2>();

		if (!Delaunay->Process(ActivePositions, ProjectionDetails))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));
			return false;
		}

		ActivePositions.Empty();

		PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::DuplicateInput);
		Edges = Delaunay->DelaunayEdges.Array();

		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
		StartParallelLoopForRange(Edges.Num());

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		PCGExGraph::FIndexedEdge E;
		const uint64 Edge = Edges[Iteration];

		uint32 A;
		uint32 B;
		PCGEx::H64(Edge, A, B);
		const bool bAIsOnHull = Delaunay->DelaunayHull.Contains(A);
		const bool bBIsOnHull = Delaunay->DelaunayHull.Contains(B);

		if (!bAIsOnHull || !bBIsOnHull)
		{
			if (!bAIsOnHull) { GraphBuilder->Graph->Nodes[A].bValid = false; }
			if (!bBIsOnHull) { GraphBuilder->Graph->Nodes[B].bValid = false; }
			return;
		}

		GraphBuilder->Graph->InsertEdge(A, B, E);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder) { return; }

		GraphBuilder->CompileAsync(AsyncManager, false);
		Context->BuildPath(GraphBuilder.Get());
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			return;
		}

		PointDataFacade->Write(AsyncManager);
		GraphBuilder->OutputEdgesToContext();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
