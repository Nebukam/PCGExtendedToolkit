// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateGraph.h"
#include "Graph/PCGExMesh.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph

int32 UPCGExBuildDelaunayGraphSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildDelaunayGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildDelaunayGraphContext::~FPCGExBuildDelaunayGraphContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(IslandsIO)
	PCGEX_DELETE(Delaunay)
	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)
}

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinIslandsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinIslandsOutput.Tooltip = FTEXT("Point data representing edges.");
#endif // WITH_EDITOR

	//PCGEx::Swap(PinProperties, PinProperties.Num() - 1, PinProperties.Num() - 2);
	return PinProperties;
}

FName UPCGExBuildDelaunayGraphSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph)

bool FPCGExBuildDelaunayGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->IslandsIO = new PCGExData::FPointIOGroup();
	Context->IslandsIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

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
		PCGEX_DELETE(Context->EdgeNetwork)
		PCGEX_DELETE(Context->Markings)
		PCGEX_DELETE(Context->Delaunay)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->Delaunay = new PCGExGeo::TDelaunayTriangulation3();
			if (Context->Delaunay->PrepareFrom(Context->CurrentIO->GetIn()->GetPoints()))
			{
				Context->CurrentIslandIO = &Context->IslandsIO->Emplace_GetRef();
				Context->EdgeNetwork = new PCGExGraph::FEdgeNetwork(Context->CurrentIO->GetNum() * 3, Context->CurrentIO->GetNum());
				Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->CurrentIO, PCGExGraph::PUIDAttributeName);

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
		Context->IslandsIO->Flush();
		Context->Markings->Mark = Context->CurrentIO->GetIn()->GetUniqueID();

		// Find unique edges
		TSet<uint64> UniqueEdges;
		TSet<int32> ConvexHull;

		TArray<PCGExGraph::FUnsignedEdge> Edges;
		UniqueEdges.Reserve(Context->Delaunay->Cells.Num() * 3);

		if (Settings->bMarkHull)
		{
			PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false);
			HullMarkPointWriter->BindAndGet(*Context->CurrentIO);
			// Find vertices that lie on the convex hull
			ConvexHull.Reserve(Context->Delaunay->Hull->Simplices.Num() * 3);
			for (const PCGExGeo::TFSimplex<4>* Simplex : Context->Delaunay->Hull->Simplices)
			{
				for (int i = 0; i < 3; i++)
				{
					if (!Simplex->Vertices[i]) { continue; }

					int32 PointIndex = Simplex->Vertices[i]->Id;
					HullMarkPointWriter->Values[PointIndex] = true;
					ConvexHull.Add(PointIndex);
				}
			}
			HullMarkPointWriter->Write();
			delete HullMarkPointWriter;
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
			MutablePoints[PointIndex].Transform.SetLocation(
				FMath::Lerp(
					(Context->Delaunay->Vertices)[EdgeStart->Values[PointIndex] = Edge.Start]->GetV3(),
					(Context->Delaunay->Vertices)[EdgeEnd->Values[PointIndex] = Edge.End]->GetV3(), 0.5));

			if (HullMarkWriter)
			{
				HullMarkWriter->Values[PointIndex] = Settings->bMarkEdgeOnTouch ?
					                                     (ConvexHull.Contains(Edge.Start) || ConvexHull.Contains(Edge.End)) :
					                                     (ConvexHull.Contains(Edge.Start) && ConvexHull.Contains(Edge.End));
			}

			PointIndex++;
		}

		EdgeStart->Write();
		EdgeEnd->Write();
		if (HullMarkWriter) { HullMarkWriter->Write(); }

		PCGEX_DELETE(EdgeStart)
		PCGEX_DELETE(EdgeEnd)
		PCGEX_DELETE(HullMarkWriter)


		Context->Markings->UpdateMark();
		Context->IslandsIO->OutputTo(Context, true);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FDelaunayInsertTask::ExecuteTask()
{
	const FPCGExBuildDelaunayGraphContext* Context = Manager->GetContext<FPCGExBuildDelaunayGraphContext>();
	//	Context->Delaunay->InsertVertex(TaskIndex);

	//PCGEX_DELETE(Triangulation3)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
