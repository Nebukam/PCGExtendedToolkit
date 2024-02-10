// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildVoronoiGraph2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExConsolidateCustomGraph.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph2D

int32 UPCGExBuildVoronoiGraph2DSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildVoronoiGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBuildVoronoiGraph2DContext::~FPCGExBuildVoronoiGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)

	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExBuildVoronoiGraph2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph2D)

bool FPCGExBuildVoronoiGraph2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	PCGEX_FWD(ProjectionSettings)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)
	PCGEX_FWD(GraphBuilderSettings)

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

			Context->GetAsyncManager()->Start<FPCGExVoronoi2Task>(Context->CurrentIO->IOIndex, Context->CurrentIO);
			Context->SetAsyncState(PCGExGeo::State_ProcessingVoronoi);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingVoronoi))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

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
		if (!Context->IsAsyncWorkComplete()) { return false; }

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

	const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
	const int32 NumPoints = Points.Num();

	TArray<FVector2D> Positions;
	Positions.SetNum(NumPoints);
	for (int i = 0; i < NumPoints; i++) { Positions[i] = FVector2D(Points[i].Transform.GetLocation()); }

	const TArrayView<FVector2D> View = MakeArrayView(Positions);
	if (!Voronoi->Process(View))
	{
		PCGEX_DELETE(Voronoi)
		return false;
	}

	TArray<FPCGPoint>& Centroids = PointIO->GetOut()->GetMutablePoints();

	const int32 NumSites = Voronoi->Centroids.Num();
	Centroids.SetNum(NumSites);

	for (int i = 0; i < NumSites; i++)
	{
		const FVector2D Centroid = Voronoi->Centroids[i];
		Centroids[i].Transform.SetLocation(FVector(Centroid.X, Centroid.Y, 0));
	}

	if (Settings->Method == EPCGExCellCenter::Circumcenter)
	{
		for (int i = 0; i < NumSites; i++)
		{
			const FVector2D Centroid = Voronoi->Circumcenters[i];
			Centroids[i].Transform.SetLocation(FVector(Centroid.X, Centroid.Y, 0));
		}
	}
	else
	{
		for (int i = 0; i < NumSites; i++)
		{
			const FVector2D Centroid = Voronoi->Centroids[i];
			Centroids[i].Transform.SetLocation(FVector(Centroid.X, Centroid.Y, 0));
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
