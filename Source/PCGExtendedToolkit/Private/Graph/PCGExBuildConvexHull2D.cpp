// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull2D

PCGExData::EInit UPCGExBuildConvexHull2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBuildConvexHull2DContext::~FPCGExBuildConvexHull2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PathsIO)
}

TArray<FPCGPinProperties> UPCGExBuildConvexHull2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	PCGEX_PIN_POINTS(PCGExGraph::OutputPathsLabel, "Point data representing closed convex hull paths.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull2D)

bool FPCGExBuildConvexHull2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	if (!Settings->GraphBuilderDetails.bPruneIsolatedPoints) { PCGEX_VALIDATE_NAME(Settings->HullAttributeName) }

	Context->PathsIO = new PCGExData::FPointIOCollection();
	Context->PathsIO->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

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

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConvexHull2D::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExConvexHull2D::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any points to build from."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->PathsIO->OutputTo(Context);
	}

	return Context->TryComplete();
}

void FPCGExBuildConvexHull2DContext::BuildPath(const PCGExGraph::FGraphBuilder* GraphBuilder) const
{
	TSet<uint64> UniqueEdges;
	const TArray<PCGExGraph::FIndexedEdge>& Edges = GraphBuilder->Graph->Edges;

	const TArray<FPCGPoint>& InPoints = GraphBuilder->PointIO->GetIn()->GetPoints();
	const PCGExData::FPointIO* PathIO = PathsIO->Emplace_GetRef(GraphBuilder->PointIO, PCGExData::EInit::NewOutput);

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
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Delaunay)

		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(HullMarkPointWriter)

		Edges.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		ProjectionDetails.Init(Context, PointDataFacade);

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Delaunay = new PCGExGeo::TDelaunay2();

		if (!Delaunay->Process(ActivePositions, ProjectionDetails))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));
			PCGEX_DELETE(Delaunay)
			return false;
		}

		ActivePositions.Empty();

		PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
		Edges = Delaunay->DelaunayEdges.Array();

		if (!Settings->GraphBuilderDetails.bPruneIsolatedPoints && Settings->bMarkHull)
		{
			HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
			HullMarkPointWriter->BindAndSetNumUninitialized(PointIO);
			StartParallelLoopForPoints();
		}

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderDetails);
		StartParallelLoopForRange(Edges.Num());

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		HullMarkPointWriter->Values[Index] = Delaunay->DelaunayHull.Contains(Index);
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

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

		GraphBuilder->CompileAsync(AsyncManagerPtr);
		TypedContext->BuildPath(GraphBuilder);
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			PCGEX_DELETE(GraphBuilder)
			PCGEX_DELETE(HullMarkPointWriter)
			return;
		}

		GraphBuilder->Write(Context);
		if (HullMarkPointWriter) { HullMarkPointWriter->Write(); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
