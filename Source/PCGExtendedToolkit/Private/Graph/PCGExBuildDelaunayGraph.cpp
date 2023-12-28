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
	PCGEX_DELETE(DelaunayTriangulation)
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

	PCGEX_FWD(IslandIDAttributeName)
	PCGEX_FWD(IslandSizeAttributeName)

	PCGEX_VALIDATE_NAME(Context->IslandIDAttributeName)
	PCGEX_VALIDATE_NAME(Context->IslandSizeAttributeName)

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
		PCGEX_DELETE(Context->DelaunayTriangulation)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->DelaunayTriangulation = new PCGExGeo::TDelaunayTriangulation3();
			if (Context->DelaunayTriangulation->PrepareFrom(*Context->CurrentIO))
			{
				Context->CurrentIslandIO = &Context->IslandsIO->Emplace_GetRef();
				Context->EdgeNetwork = new PCGExGraph::FEdgeNetwork(Context->CurrentIO->GetNum() * 3, Context->CurrentIO->GetNum());
				Context->Markings = new PCGExData::FKPointIOMarkedBindings<int32>(Context->CurrentIO, PCGExGraph::PUIDAttributeName);

				Context->DelaunayTriangulation->Generate();
				Context->SetState(PCGExMT::State_ProcessingPoints);
			}
			else
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			//(*Context->Delaunay->Simplices.Find(0))->Draw(Context->World);
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FDelaunayInsertTask>(PointIndex, Context->CurrentIO);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->SetState(PCGExGraph::State_WritingIslands);
		}
	}

	// -> Delaunay is ready

	if (Context->IsState(PCGExGraph::State_WritingIslands))
	{
		Context->IslandsIO->Flush();
		Context->Markings->Mark = Context->CurrentIO->GetIn()->GetUniqueID();

		// Find unique edges
		TSet<uint64> UniqueEdges;
		TArray<PCGExGraph::FUnsignedEdge> Edges;
		UniqueEdges.Reserve(Context->DelaunayTriangulation->Cells.Num() * 3);
		for (const PCGExGeo::TDelaunayCell<4, FVector4>* Cell : Context->DelaunayTriangulation->Cells)
		{
			for (int i = 0; i < 4; i++)
			{
				const int32 A = Cell->Simplex->Vertices[i]->Id;
				for (int j = i + 1; j < 4; j++)
				{
					const int32 B = Cell->Simplex->Vertices[j]->Id;
					const uint64 Hash = PCGExGraph::GetUnsignedHash64(A, B);
					if (UniqueEdges.Contains(Hash)) { Edges.Emplace(A, B, EPCGExEdgeType::Complete); }
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
					(Context->DelaunayTriangulation->Vertices)[EdgeStart->Values[PointIndex] = Edge.Start]->Position,
					(Context->DelaunayTriangulation->Vertices)[EdgeEnd->Values[PointIndex] = Edge.End]->Position, 0.5));
			PointIndex++;
		}

		EdgeStart->Write();
		EdgeEnd->Write();

		PCGEX_DELETE(EdgeStart)
		PCGEX_DELETE(EdgeEnd)

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
