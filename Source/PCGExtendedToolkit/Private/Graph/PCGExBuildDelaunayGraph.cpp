// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateGraph.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph

int32 UPCGExBuildDelaunayGraphSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildDelaunayGraphSettings::GetMainOutputInitMode() const { return bMarkHull ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward; }

FPCGExBuildDelaunayGraphContext::~FPCGExBuildDelaunayGraphContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(ClustersIO)

	PCGEX_DELETE(Delaunay)
	PCGEX_DELETE(ConvexHull)

	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)

	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExBuildDelaunayGraphSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph)

bool FPCGExBuildDelaunayGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->ClustersIO = new PCGExData::FPointIOGroup();
	Context->ClustersIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

	return true;
}

bool FPCGExBuildDelaunayGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildDelaunayGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->Delaunay)
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

			if (Settings->bMarkHull)
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
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Delaunay 2D instead."));
					Context->SetState(PCGExMT::State_ReadyForNextPoints);
					return false;
				}
			}

			bool bValidDelaunay = false;

			Context->Delaunay = new PCGExGeo::TDelaunayTriangulation3();
			if (Context->Delaunay->PrepareFrom(Context->CurrentIO->GetIn()->GetPoints()))
			{
				Context->Delaunay->Generate();
				bValidDelaunay = !Context->Delaunay->Cells.IsEmpty();
				Context->SetState(PCGExGraph::State_WritingClusters);
			}

			if (!bValidDelaunay)
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Delaunay 2D instead."));
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

void FPCGExBuildDelaunayGraphElement::WriteEdges(FPCGExBuildDelaunayGraphContext* Context) const
{
	PCGEX_SETTINGS(BuildDelaunayGraph)

	// Find unique edges
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;
	Context->Delaunay->GetUniqueEdges(Edges);

	PCGExData::FPointIO& DelaunayEdges = Context->ClustersIO->Emplace_GetRef();
	Context->Markings->Add(DelaunayEdges);

	TArray<FPCGPoint>& MutablePoints = DelaunayEdges.GetOut()->GetMutablePoints();
	MutablePoints.SetNum(Edges.Num());

	DelaunayEdges.CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);
	PCGEx::TFAttributeWriter<bool>* HullMarkWriter = nullptr;

	EdgeStart->BindAndGet(DelaunayEdges);
	EdgeEnd->BindAndGet(DelaunayEdges);

	if (Settings->bMarkHull)
	{
		HullMarkWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false);
		HullMarkWriter->BindAndGet(DelaunayEdges);
	}

	int32 PointIndex = 0;
	for (const PCGExGraph::FUnsignedEdge& Edge : Edges)
	{
		EdgeStart->Values[PointIndex] = Edge.Start;
		EdgeEnd->Values[PointIndex] = Edge.End;

		const FVector StartPosition = Context->CurrentIO->GetInPoint(Edge.Start).Transform.GetLocation();
		const FVector EndPosition = Context->CurrentIO->GetInPoint(Edge.End).Transform.GetLocation();

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
