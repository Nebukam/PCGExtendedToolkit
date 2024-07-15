// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull

PCGExData::EInit UPCGExBuildConvexHullSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExBuildConvexHullContext::~FPCGExBuildConvexHullContext()
{
	PCGEX_TERMINATE_ASYNC
}

TArray<FPCGPinProperties> UPCGExBuildConvexHullSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull)

bool FPCGExBuildConvexHullElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)

	if (!Settings->GraphBuilderDetails.bPruneIsolatedPoints) { PCGEX_VALIDATE_NAME(Settings->HullAttributeName) }

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

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConvexHull::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 4)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExConvexHull::FProcessor>* NewBatch)
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
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 4 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExConvexHull
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConvexHull::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildConvexHull)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Delaunay = new PCGExGeo::TDelaunay3();

		if (!Delaunay->Process(ActivePositions, false))
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

		GraphBuilder->CompileAsync(AsyncManagerPtr);
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
