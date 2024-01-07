// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph2D.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateGraph.h"
#include "Graph/PCGExMesh.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph2D

int32 UPCGExBuildDelaunayGraph2DSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildDelaunayGraph2DSettings::GetMainOutputInitMode() const { return bHullOnly ? PCGExData::EInit::NewOutput : PCGExData::EInit::DuplicateInput; }

FPCGExBuildDelaunayGraph2DContext::~FPCGExBuildDelaunayGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(IslandsIO)
	PCGEX_DELETE(Delaunay)
	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)
}

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinIslandsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinIslandsOutput.Tooltip = FTEXT("Point data representing edges.");
#endif // WITH_EDITOR

	//PCGEx::Swap(PinProperties, PinProperties.Num() - 1, PinProperties.Num() - 2);
	return PinProperties;
}

FName UPCGExBuildDelaunayGraph2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph2D)

bool FPCGExBuildDelaunayGraph2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->IslandsIO = new PCGExData::FPointIOGroup();
	Context->IslandsIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

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
		PCGEX_DELETE(Context->Delaunay)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->Delaunay = new PCGExGeo::TDelaunayTriangulation3();
			if (Context->Delaunay->PrepareFrom(Context->CurrentIO->GetIn()->GetPoints()))
			{
				Context->Delaunay->Generate();
				Context->SetState(PCGExGraph::State_WritingIslands);
			}
			else
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_WritingIslands))
	{
		Context->EdgeNetwork = new PCGExGraph::FEdgeNetwork(Context->CurrentIO->GetNum() * 3, Context->CurrentIO->GetNum());
		Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->CurrentIO, PCGExGraph::PUIDAttributeName);
		Context->Markings->Mark = Context->CurrentIO->GetIn()->GetUniqueID();

		if (!Settings->bHullOnly) { ExportCurrent(Context); }
		else { ExportCurrentHullOnly(Context); }

		Context->Markings->UpdateMark();
		Context->IslandsIO->OutputTo(Context, true);
		Context->IslandsIO->Flush();

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

void FPCGExBuildDelaunayGraph2DElement::ExportCurrent(FPCGExBuildDelaunayGraph2DContext* Context) const
{
	PCGEX_SETTINGS(BuildDelaunayGraph2D)

	// Find unique edges
	TSet<uint64> UniqueEdges;
	TMap<int32, int32> IndicesRemap;

	TArray<PCGExGraph::FUnsignedEdge> Edges;
	UniqueEdges.Reserve(Context->Delaunay->Cells.Num() * 3);

	if (Settings->bMarkHull)
	{
		PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
		HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

		for (const PCGExGeo::TFVtx<4>* Vtx : Context->Delaunay->Vertices) { HullMarkPointWriter->Values[Vtx->Id] = Vtx->bIsOnHull; }

		HullMarkPointWriter->Write();
		PCGEX_DELETE(HullMarkPointWriter)
	}

	// Find delaunay edges
	for (const PCGExGeo::TDelaunayCell<4>* Cell : Context->Delaunay->Cells)
	{
		for (int i = 0; i < 4; i++)
		{
			const int32 A = Cell->Simplex->Vertices[i]->Id;
			for (int j = i + 1; j < 4; j++)
			{
				const int32 B = Cell->Simplex->Vertices[j]->Id;
				const uint64 Hash = PCGExGraph::GetUnsignedHash64(A, B);
				if (!UniqueEdges.Contains(Hash))
				{
					Edges.Emplace(A, B, EPCGExEdgeType::Complete);
					UniqueEdges.Add(Hash);
				}
			}
		}
	}

	PCGExData::FPointIO& DelaunayEdges = Context->IslandsIO->Emplace_GetRef();
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
		const PCGExGeo::TFVtx<4>* StartVtx = Context->Delaunay->Vertices[Edge.Start];
		const PCGExGeo::TFVtx<4>* EndVtx = Context->Delaunay->Vertices[Edge.End];
		
		MutablePoints[PointIndex].Transform.SetLocation(FMath::Lerp(StartVtx->GetV3(), EndVtx->GetV3(), 0.5));
		
		EdgeStart->Values[PointIndex] = Edge.Start;
		EdgeEnd->Values[PointIndex] = Edge.End;
		
		if (HullMarkWriter)
		{
			HullMarkWriter->Values[PointIndex] = Settings->bMarkEdgeOnTouch ?
				                                     StartVtx->bIsOnHull || EndVtx->bIsOnHull :
				                                     StartVtx->bIsOnHull && EndVtx->bIsOnHull;
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

void FPCGExBuildDelaunayGraph2DElement::ExportCurrentHullOnly(FPCGExBuildDelaunayGraph2DContext* Context) const
{
	PCGEX_SETTINGS(BuildDelaunayGraph2D)

	// Find unique edges
	TSet<uint64> UniqueEdges;
	TMap<int32, int32> IndicesRemap;
	TArray<PCGExGraph::FUnsignedEdge> Edges;

	int32 TruncatedIndex = 0;

	UniqueEdges.Reserve(Context->Delaunay->Cells.Num() * 3);

	const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
	TArray<FPCGPoint>& OutPoints = Context->CurrentIO->GetOut()->GetMutablePoints();
	OutPoints.Reserve(Context->Delaunay->Hull->Simplices.Num() * 3);

	// Find vertices that lie on the convex hull
	for (const PCGExGeo::TFVtx<4>* Vtx : Context->Delaunay->Vertices)
	{
		int32 VId = Vtx->Id;
		if (!Vtx->bIsOnHull || IndicesRemap.Contains(VId)) { continue; }
		IndicesRemap.Add(VId, OutPoints.Add(InPoints[VId]));
	}

	// Find delaunay edges
	for (const PCGExGeo::TDelaunayCell<4>* Cell : Context->Delaunay->Cells)
	{
		for (int i = 0; i < 4; i++)
		{
			const int32* A = IndicesRemap.Find(Cell->Simplex->Vertices[i]->Id);
			for (int j = i + 1; j < 4; j++)
			{
				const int32* B = IndicesRemap.Find(Cell->Simplex->Vertices[j]->Id);

				if (!A || !B) { continue; } // Not on the hull

				const uint64 Hash = PCGExGraph::GetUnsignedHash64(*A, *B);
				if (!UniqueEdges.Contains(Hash))
				{
					Edges.Emplace(*A, *B, EPCGExEdgeType::Complete);
					UniqueEdges.Add(Hash);
				}
			}
		}
	}

	PCGExData::FPointIO& DelaunayEdges = Context->IslandsIO->Emplace_GetRef();
	Context->Markings->Add(DelaunayEdges);

	TArray<FPCGPoint>& MutablePoints = DelaunayEdges.GetOut()->GetMutablePoints();
	MutablePoints.SetNum(Edges.Num());

	DelaunayEdges.CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);

	EdgeStart->BindAndGet(DelaunayEdges);
	EdgeEnd->BindAndGet(DelaunayEdges);

	int32 PointIndex = 0;
	for (const PCGExGraph::FUnsignedEdge& Edge : Edges)
	{
		MutablePoints[PointIndex].Transform.SetLocation(
			FMath::Lerp(
				OutPoints[EdgeStart->Values[PointIndex] = Edge.Start].Transform.GetLocation(),
				OutPoints[EdgeEnd->Values[PointIndex] = Edge.End].Transform.GetLocation(), 0.5));

		PointIndex++;
	}

	EdgeStart->Write();
	EdgeEnd->Write();

	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)
}

bool FDelaunay2DInsertTask::ExecuteTask()
{
	const FPCGExBuildDelaunayGraph2DContext* Context = Manager->GetContext<FPCGExBuildDelaunayGraph2DContext>();
	//	Context->Delaunay->InsertVertex(TaskIndex);

	//PCGEX_DELETE(Triangulation3)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
