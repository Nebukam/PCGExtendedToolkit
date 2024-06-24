// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph2D

namespace PCGExGeoTask
{
	class FLloydRelax2;
}

PCGExData::EInit UPCGExBuildDelaunayGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExBuildDelaunayGraph2DContext::~FPCGExBuildDelaunayGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC
}

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExBuildDelaunayGraph2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph2D)

bool FPCGExBuildDelaunayGraph2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	return true;
}

bool FPCGExBuildDelaunayGraph2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildDelaunayGraph2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildDelaunay2D::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBuildDelaunay2D::FProcessor>* NewBatch)
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
	}

	return Context->CompleteTaskExecution();
}

namespace PCGExBuildDelaunay2D
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Delaunay)

		ProjectionSettings.Cleanup();
		PCGEX_DELETE(GraphBuilder)

		PCGEX_DELETE(HullMarkPointWriter)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		ProjectionSettings = Settings->ProjectionSettings;
		ProjectionSettings.Init(PointIO);

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Delaunay = new PCGExGeo::TDelaunay2();

		if (!Delaunay->Process(ActivePositions, ProjectionSettings))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generated invalid results."));
			PCGEX_DELETE(Delaunay)
			return false;
		}

		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::DuplicateInput);

		if (Settings->bUrquhart) { Delaunay->RemoveLongestEdges(ActivePositions); }
		if (Settings->bMarkHull) { HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false); }

		ActivePositions.Empty();

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderSettings);
		GraphBuilder->Graph->InsertEdges(Delaunay->DelaunayEdges, -1);

		GraphBuilder->CompileAsync(AsyncManagerPtr);

		if (!Settings->bMarkHull) { PCGEX_DELETE(Delaunay) }

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
	{
		HullMarkPointWriter->Values[Index] = Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			PCGEX_DELETE(GraphBuilder)
			PCGEX_DELETE(HullMarkPointWriter)
			return;
		}

		GraphBuilder->Write(Context);

		if (HullMarkPointWriter)
		{
			HullMarkPointWriter->BindAndSetNumUninitialized(PointIO);
			StartParallelLoopForPoints();
		}
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder) { return; }
		if (HullMarkPointWriter) { PCGEX_ASYNC_WRITE_DELETE(AsyncManagerPtr, HullMarkPointWriter) }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
