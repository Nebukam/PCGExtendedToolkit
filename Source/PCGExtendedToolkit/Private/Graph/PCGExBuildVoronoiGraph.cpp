// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildVoronoiGraph.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExConsolidateGraph.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph

int32 UPCGExBuildVoronoiGraphSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildVoronoiGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBuildVoronoiGraphContext::~FPCGExBuildVoronoiGraphContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(ClustersIO)

	PCGEX_DELETE(Voronoi)
	PCGEX_DELETE(ConvexHull)

	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)

	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExBuildVoronoiGraphSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph)

bool FPCGExBuildVoronoiGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->ClustersIO = new PCGExData::FPointIOGroup();
	Context->ClustersIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

	return true;
}

bool FPCGExBuildVoronoiGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->Voronoi)
		PCGEX_DELETE(Context->ConvexHull)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 4)
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have too few points to be processed (<= 4)."));
				return false;
			}

			if (false) //if (Settings->bMarkHull)
			{
				Context->ConvexHull = new PCGExGeo::TConvexHull3();
				TArray<PCGExGeo::TFVtx<3>*> HullVertices;
				GetVerticesFromPoints(Context->CurrentIO->GetIn()->GetPoints(), HullVertices);

				if (Context->ConvexHull->Prepare(HullVertices))
				{
					Context->ConvexHull->Generate();
					Context->ConvexHull->GetHullIndices(Context->HullIndices);

					PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
					HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

					for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

					HullMarkPointWriter->Write();
					PCGEX_DELETE(HullMarkPointWriter)

					PCGEX_DELETE(Context->ConvexHull)
				}
				else
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Voronoi 2D instead."));
					Context->SetState(PCGExMT::State_ReadyForNextPoints);
					return false;
				}
			}

			bool bValidVoronoi = false;

			Context->Voronoi = new PCGExGeo::TVoronoiMesh3();
			Context->Voronoi->CellCenter = Settings->Method;
			Context->Voronoi->BoundsExtension = Settings->BoundsCutoff;

			if (Context->Voronoi->PrepareFrom(Context->CurrentIO->GetIn()->GetPoints()))
			{
				Context->Voronoi->Generate();
				bValidVoronoi = !Context->Voronoi->Regions.IsEmpty();
				Context->SetState(PCGExGraph::State_WritingClusters);
			}

			if (!bValidVoronoi)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Voronoi 2D instead."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}
			Context->SetState(PCGExGraph::State_WritingClusters);
		}
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		Context->EdgeNetwork = new PCGExGraph::FEdgeNetwork(10, Context->CurrentIO->GetNum());
		Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->CurrentIO, PCGExGraph::PUIDAttributeName);
		Context->Markings->Mark = Context->CurrentIO->GetIn()->GetUniqueID();

		WriteEdges(Context);

		Context->Markings->UpdateMark();
		Context->ClustersIO->OutputTo(Context, true);
		Context->ClustersIO->Flush();

		PCGEX_DELETE(Context->EdgeNetwork)
		PCGEX_DELETE(Context->Markings)

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

void FPCGExBuildVoronoiGraphElement::WriteEdges(FPCGExBuildVoronoiGraphContext* Context) const
{
	PCGEX_SETTINGS(BuildVoronoiGraph)

	// Vtx -> Circumcenters
	//TODO : Datablending

	TArray<FPCGPoint>& Centroids = Context->CurrentIO->GetOut()->GetMutablePoints();
	Centroids.SetNum(Context->Voronoi->Delaunay->Cells.Num());

	switch (Settings->Method)
	{
	default:
	case EPCGExCellCenter::Balanced:
		for (const PCGExGeo::TDelaunayCell<4>* Cell : Context->Voronoi->Delaunay->Cells)
		{
			const int32 CellIndex = Cell->Circumcenter->Id;
			Centroids[CellIndex].Transform.SetLocation(Cell->GetBestCenter());
		}
		break;
	case EPCGExCellCenter::Circumcenter:
		for (const PCGExGeo::TDelaunayCell<4>* Cell : Context->Voronoi->Delaunay->Cells)
		{
			const int32 CellIndex = Cell->Circumcenter->Id;
			Centroids[CellIndex].Transform.SetLocation(Cell->Circumcenter->GetV3());
		}
		break;
	case EPCGExCellCenter::Centroid:
		for (const PCGExGeo::TDelaunayCell<4>* Cell : Context->Voronoi->Delaunay->Cells)
		{
			const int32 CellIndex = Cell->Circumcenter->Id;
			Centroids[CellIndex].Transform.SetLocation(Cell->Centroid);
		}
		break;
	}

	// Find unique edges
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;
	Context->Voronoi->GetUniqueEdges(Edges, Settings->bPruneOutsideBounds && Settings->Method != EPCGExCellCenter::Balanced);

	PCGExData::FPointIO& VoronoiEdges = Context->ClustersIO->Emplace_GetRef();
	Context->Markings->Add(VoronoiEdges);

	TArray<FPCGPoint>& MutablePoints = VoronoiEdges.GetOut()->GetMutablePoints();
	MutablePoints.SetNum(Edges.Num());

	VoronoiEdges.CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);
	PCGEx::TFAttributeWriter<bool>* HullMarkWriter = nullptr;

	EdgeStart->BindAndGet(VoronoiEdges);
	EdgeEnd->BindAndGet(VoronoiEdges);

	if (false) //if (Settings->bMarkHull)
	{
		HullMarkWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false);
		HullMarkWriter->BindAndGet(VoronoiEdges);
	}

	int32 PointIndex = 0;
	for (const PCGExGraph::FUnsignedEdge& Edge : Edges)
	{
		EdgeStart->Values[PointIndex] = Edge.Start;
		EdgeEnd->Values[PointIndex] = Edge.End;

		const FVector StartPosition = Context->CurrentIO->GetOutPoint(Edge.Start).Transform.GetLocation();
		const FVector EndPosition = Context->CurrentIO->GetOutPoint(Edge.End).Transform.GetLocation();

		MutablePoints[PointIndex].Transform.SetLocation(FMath::Lerp(StartPosition, EndPosition, 0.5));

		if (HullMarkWriter)
		{
			const bool bStartOnHull = Context->HullIndices.Contains(Edge.Start);
			const bool bEndOnHull = Context->HullIndices.Contains(Edge.End);
			HullMarkWriter->Values[PointIndex] = Settings->bMarkEdgeOnTouch ? bStartOnHull || bEndOnHull : bStartOnHull && bEndOnHull;
		}

		PointIndex++;
	}

	EdgeStart->Write();
	EdgeEnd->Write();
	if (HullMarkWriter) { HullMarkWriter->Write(); }

	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)
	PCGEX_DELETE(HullMarkWriter)
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
