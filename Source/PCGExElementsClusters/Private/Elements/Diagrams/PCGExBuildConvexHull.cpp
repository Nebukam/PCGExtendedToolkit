// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Diagrams/PCGExBuildConvexHull.h"


#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Math/Geo/PCGExDelaunay.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"

#define LOCTEXT_NAMESPACE "PCGExGraphs"
#define PCGEX_NAMESPACE BuildConvexHull

TArray<FPCGPinProperties> UPCGExBuildConvexHullSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExClusters::Labels::OutputEdgesLabel, "Point data representing edges.", Required)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull)

PCGExData::EIOInit UPCGExBuildConvexHullSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(BuildConvexHull)

FName UPCGExBuildConvexHullSettings::GetMainOutputPin() const { return PCGExClusters::Labels::OutputVerticesLabel; }

bool FPCGExBuildConvexHullElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)

	return true;
}

bool FPCGExBuildConvexHullElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHullElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 4 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry)
		                                         {
			                                         if (Entry->GetNum() < 4)
			                                         {
				                                         bHasInvalidInputs = true;
				                                         return false;
			                                         }
			                                         return true;
		                                         }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		                                         {
			                                         NewBatch->bRequiresWriteStep = true;
		                                         }))
		{
			return Context->CancelExecution(TEXT("Could not find any valid inputs to build from."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	Context->MainBatch->Output();

	return Context->TryComplete();
}

namespace PCGExBuildConvexHull
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildConvexHull::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExPointArrayDataHelpers::PointsToPositions(PointDataFacade->GetIn(), ActivePositions);

		Delaunay = MakeShared<PCGExMath::Geo::TDelaunay3>();

		if (!Delaunay->Process<false, true>(ActivePositions))
		{
			if (!Context->bQuietInvalidInputWarning)
			{
				PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));
			}
			return false;
		}

		ActivePositions.Empty();

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		Edges = Delaunay->DelaunayEdges.Array();

		GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
		StartParallelLoopForRange(Edges.Num());

		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			PCGExGraphs::FEdge E;
			const uint64 Edge = Edges[Index];

			uint32 A;
			uint32 B;
			PCGEx::H64(Edge, A, B);
			const bool bAIsOnHull = Delaunay->DelaunayHull.Contains(A);
			const bool bBIsOnHull = Delaunay->DelaunayHull.Contains(B);

			if (!bAIsOnHull || !bBIsOnHull)
			{
				if (!bAIsOnHull) { GraphBuilder->Graph->Nodes[A].bValid = false; }
				if (!bBIsOnHull) { GraphBuilder->Graph->Nodes[B].bValid = false; }
				continue;
			}

			GraphBuilder->Graph->InsertEdge(A, B, E);
		}
	}

	void FProcessor::CompleteWork()
	{
		GraphBuilder->CompileAsync(TaskManager, false);
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		PointDataFacade->WriteFastest(TaskManager);
	}

	void FProcessor::Output()
	{
		GraphBuilder->StageEdgesOutputs();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
