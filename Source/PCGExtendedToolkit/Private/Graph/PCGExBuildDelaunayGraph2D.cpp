// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph2D

namespace PCGExGeoTask
{
	class FLloydRelax2;
}

PCGExData::EInit UPCGExBuildDelaunayGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildDelaunayGraph2DContext::~FPCGExBuildDelaunayGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	HullIndices.Empty();

	ProjectionSettings.Cleanup();
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
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

	Context->GraphBuilderSettings.bPruneIsolatedPoints = false;

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	PCGEX_FWD(ProjectionSettings)

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
			Context->GetAsyncManager()->Start<FPCGExDelaunay2Task>(Context->CurrentIO->IOIndex, Context->CurrentIO, Context->GraphBuilder->Graph);

			Context->SetAsyncState(PCGExGeo::State_ProcessingDelaunay);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunay))
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
			if (Settings->bMarkHull)
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

bool FPCGExDelaunay2Task::ExecuteTask()
{
	FPCGExBuildDelaunayGraph2DContext* Context = static_cast<FPCGExBuildDelaunayGraph2DContext*>(Manager->Context);
	PCGEX_SETTINGS(BuildDelaunayGraph2D)

	PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();

	TArray<FVector> ActivePositions;
	PCGExGeo::PointsToPositions(Context->CurrentIO->GetIn()->GetPoints(), ActivePositions);

	const TArrayView<FVector> View = MakeArrayView(ActivePositions);
	if (!Delaunay->Process(View, Context->ProjectionSettings))
	{
		ActivePositions.Empty();
		PCGEX_DELETE(Delaunay)
		return false;
	}

	if (Settings->bUrquhart) { Delaunay->RemoveLongestEdges(View); }
	if (Settings->bMarkHull) { Context->HullIndices.Append(Delaunay->DelaunayHull); }

	Graph->InsertEdges(Delaunay->DelaunayEdges, -1);

	ActivePositions.Empty();
	PCGEX_DELETE(Delaunay)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
