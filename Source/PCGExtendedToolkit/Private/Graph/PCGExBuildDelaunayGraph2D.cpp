// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph2D.h"

#include "CompGeom/Delaunay3.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateCustomGraph.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph2D

int32 UPCGExBuildDelaunayGraph2DSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildDelaunayGraph2DSettings::GetMainOutputInitMode() const { return bMarkHull ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward; }

FPCGExBuildDelaunayGraph2DContext::~FPCGExBuildDelaunayGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExBuildDelaunayGraph2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph2D)

bool FPCGExBuildDelaunayGraph2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

	PCGEX_FWD(ProjectionSettings)
	Context->GraphBuilderSettings.bPruneIsolatedPoints = false;
	
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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->GraphBuilder)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 4)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6);
			Context->GetAsyncManager()->Start<FPCGExDelaunay2Task>(Context->CurrentIO->IOIndex, Context->CurrentIO, Context->GraphBuilder->Graph);

			Context->SetAsyncState(PCGExGeo::State_ProcessingDelaunay);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunay))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

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
		if (!Context->IsAsyncWorkComplete()) { return false; }
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
	PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();

	const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = Points.Num();

	TArray<FVector2D> Positions;
	Positions.SetNum(NumPoints);
	for (int i = 0; i < NumPoints; i++)
	{
		const FVector Pos = Points[i].Transform.GetLocation();
		Positions[i] = FVector2D(Pos.X, Pos.Y);
	}

	const TArrayView<FVector2D> View = MakeArrayView(Positions);
	if (!Delaunay->Process(View))
	{
		PCGEX_DELETE(Delaunay)
		return false;
	}

	Graph->InsertEdges(Delaunay->DelaunayEdges, -1);
	
	PCGEX_DELETE(Delaunay)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
