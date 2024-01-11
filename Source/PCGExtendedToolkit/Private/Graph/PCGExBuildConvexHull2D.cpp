// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull2D.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateGraph.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull2D

int32 UPCGExBuildConvexHull2DSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildConvexHull2DSettings::GetMainOutputInitMode() const
{
	return bPrunePoints ? PCGExData::EInit::NewOutput : bMarkHull ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward;
}

FPCGExBuildConvexHull2DContext::~FPCGExBuildConvexHull2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(ClustersIO)
	PCGEX_DELETE(PathsIO)

	PCGEX_DELETE(ConvexHull)

	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)

	HullIndices.Empty();
	IndicesRemap.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildConvexHull2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif

	FPCGPinProperties& PinPathsOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPathsLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPathsOutput.Tooltip = FTEXT("Point data representing closed convex hull paths.");
#endif

	return PinProperties;
}

FName UPCGExBuildConvexHull2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull2D)

bool FPCGExBuildConvexHull2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->ClustersIO = new PCGExData::FPointIOGroup();
	Context->ClustersIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

	Context->PathsIO = new PCGExData::FPointIOGroup();
	Context->PathsIO->DefaultOutputLabel = PCGExGraph::OutputPathsLabel;

	return true;
}

bool FPCGExBuildConvexHull2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHull2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->ConvexHull)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 3)
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->ConvexHull = new PCGExGeo::TConvexHull2();
			TArray<PCGExGeo::TFVtx<2>*> HullVertices;
			const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
			GetVerticesFromPoints(InPoints, HullVertices);

			if (Context->ConvexHull->Prepare(HullVertices))
			{
				Context->ConvexHull->Generate();
				Context->ConvexHull->GetHullIndices(Context->HullIndices);

				if (Settings->bPrunePoints)
				{
					TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();
					MutablePoints.SetNumUninitialized(Context->HullIndices.Num());
					int32 PointIndex = 0;

					for (int i = 0; i < Context->CurrentIO->GetNum(); i++)
					{
						if (!Context->HullIndices.Contains(i)) { continue; }
						MutablePoints[PointIndex] = InPoints[i];
						Context->IndicesRemap.Add(i, PointIndex++);
					}
				}
				else if (Settings->bMarkHull)
				{
					PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
					HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

					for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

					HullMarkPointWriter->Write();
					PCGEX_DELETE(HullMarkPointWriter)
				}
			}
			else
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs generates no results. Check for singularities."));
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
		Context->PathsIO->OutputTo(Context);
	}

	return Context->IsDone();
}

void FPCGExBuildConvexHull2DElement::WriteEdges(FPCGExBuildConvexHull2DContext* Context) const
{
	PCGEX_SETTINGS(BuildConvexHull2D)

	// Find unique edges
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;

	Context->ConvexHull->GetUniqueEdges(Edges);

	PCGExData::FPointIO& PathIO = Context->PathsIO->Emplace_GetRef(*Context->CurrentIO, PCGExData::EInit::NewOutput);
	Context->Markings->Add(PathIO);

	TArray<FPCGPoint>& MutablePathPoints = PathIO.GetOut()->GetMutablePoints();
	TSet<int32> VisitedEdges;
	VisitedEdges.Reserve(Edges.Num());

	int32 CurrentIndex = -1;
	int32 FirstIndex = -1;

	while (MutablePathPoints.Num() != Edges.Num())
	{
		int32 EdgeIndex = -1;

		if (CurrentIndex == -1)
		{
			EdgeIndex = 0;
			FirstIndex = Edges[EdgeIndex].Start;
			MutablePathPoints.Add(Context->CurrentIO->GetInPoint(FirstIndex));
			CurrentIndex = Edges[EdgeIndex].End;
		}
		else
		{
			for (int i = 0; i < Edges.Num(); i++)
			{
				EdgeIndex = i;
				if (VisitedEdges.Contains(EdgeIndex)) { continue; }

				const PCGExGraph::FUnsignedEdge& Edge = Edges[EdgeIndex];
				if (!Edge.Contains(CurrentIndex)) { continue; }

				CurrentIndex = Edge.Other(CurrentIndex);
				break;
			}
		}

		if (CurrentIndex == FirstIndex) { break; }

		VisitedEdges.Add(EdgeIndex);
		MutablePathPoints.Add(Context->CurrentIO->GetInPoint(CurrentIndex));
	}

	VisitedEdges.Empty();

	//		

	if (Settings->bPrunePoints)
	{
		Edges.Empty(Edges.Num());
		Context->ConvexHull->GetUniqueEdgesRemapped(Edges, Context->IndicesRemap);
	}

	PCGExData::FPointIO& HullEdges = Context->ClustersIO->Emplace_GetRef();
	Context->Markings->Add(HullEdges);

	TArray<FPCGPoint>& MutablePoints = HullEdges.GetOut()->GetMutablePoints();
	MutablePoints.SetNum(Edges.Num());

	HullEdges.CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);

	EdgeStart->BindAndGet(HullEdges);
	EdgeEnd->BindAndGet(HullEdges);

	int32 PointIndex = 0;
	for (const PCGExGraph::FUnsignedEdge& Edge : Edges)
	{
		EdgeStart->Values[PointIndex] = Edge.Start;
		EdgeEnd->Values[PointIndex] = Edge.End;

		const FVector StartPosition = Context->CurrentIO->GetOutPoint(Edge.Start).Transform.GetLocation();
		const FVector EndPosition = Context->CurrentIO->GetOutPoint(Edge.End).Transform.GetLocation();

		MutablePoints[PointIndex].Transform.SetLocation(FMath::Lerp(StartPosition, EndPosition, 0.5));

		PointIndex++;
	}

	EdgeStart->Write();
	EdgeEnd->Write();

	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)
}

bool FHull2DInsertTask::ExecuteTask()
{
	const FPCGExBuildConvexHull2DContext* Context = Manager->GetContext<FPCGExBuildConvexHull2DContext>();
	//	Context->Delaunay->InsertVertex(TaskIndex);

	//PCGEX_DELETE(Triangulation3)
	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
