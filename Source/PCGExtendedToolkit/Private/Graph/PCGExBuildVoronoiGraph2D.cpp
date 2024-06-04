// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildVoronoiGraph2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph2D

PCGExData::EInit UPCGExBuildVoronoiGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBuildVoronoiGraph2DContext::~FPCGExBuildVoronoiGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	HullIndices.Empty();

	ProjectionSettings.Cleanup();
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExBuildVoronoiGraph2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph2D)

bool FPCGExBuildVoronoiGraph2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)
	PCGEX_FWD(GraphBuilderSettings)

	PCGEX_FWD(ProjectionSettings)

	return true;
}

bool FPCGExBuildVoronoiGraph2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraph2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

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
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->ProjectionSettings.Init(Context->CurrentIO);

			Context->GetAsyncManager()->Start<FPCGExVoronoi2Task>(Context->CurrentIO->IOIndex, Context->CurrentIO);
			Context->SetAsyncState(PCGExGeo::State_ProcessingVoronoi);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingVoronoi))
	{
		PCGEX_WAIT_ASYNC

		if (!Context->GraphBuilder || Context->GraphBuilder->Graph->Edges.IsEmpty())
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

		if (Context->GraphBuilder->bCompiledSuccessfully) { Context->GraphBuilder->Write(Context); }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExVoronoi2Task::ExecuteTask()
{
	FPCGExBuildVoronoiGraph2DContext* Context = static_cast<FPCGExBuildVoronoiGraph2DContext*>(Manager->Context);
	PCGEX_SETTINGS(BuildVoronoiGraph2D)

	PCGExGeo::TVoronoi2* Voronoi = new PCGExGeo::TVoronoi2();

	TArray<FVector> ActivePositions;
	PCGExGeo::PointsToPositions(Context->CurrentIO->GetIn()->GetPoints(), ActivePositions);

	if (const TArrayView<FVector> View = MakeArrayView(ActivePositions);
		!Voronoi->Process(View, Context->ProjectionSettings))
	{
		ActivePositions.Empty();
		PCGEX_DELETE(Voronoi)
		return false;
	}

	ActivePositions.Empty();

	TArray<FPCGPoint>& Centroids = PointIO->GetOut()->GetMutablePoints();

	const int32 NumSites = Voronoi->Centroids.Num();
	Centroids.SetNum(NumSites);

	if (Settings->Method == EPCGExCellCenter::Circumcenter)
	{
		for (int i = 0; i < NumSites; i++) { Centroids[i].Transform.SetLocation(Voronoi->Circumcenters[i]); }
	}
	else if (Settings->Method == EPCGExCellCenter::Centroid)
	{
		for (int i = 0; i < NumSites; i++) { Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]); }
	}
	else if (Settings->Method == EPCGExCellCenter::Balanced)
	{
		const FBox Bounds = PointIO->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);
		for (int i = 0; i < NumSites; i++)
		{
			FVector Target = Voronoi->Circumcenters[i];
			if (Bounds.IsInside(Target)) { Centroids[i].Transform.SetLocation(Target); }
			else { Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]); }
		}
	}

	//if (Settings->bMarkHull) { Context->HullIndices.Append(Voronoi->DelaunayHull); }
	Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*PointIO, &Context->GraphBuilderSettings, 6);
	Context->GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);

	PCGEX_DELETE(Voronoi)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
