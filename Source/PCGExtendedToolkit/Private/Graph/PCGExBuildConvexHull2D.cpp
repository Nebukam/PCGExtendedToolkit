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

	PCGEX_DELETE(ConvexHull)
	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(PathsIO)

	HullIndices.Empty();
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
		PCGEX_DELETE(Context->GraphBuilder)
		PCGEX_DELETE(Context->ConvexHull)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 3)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->ConvexHull = new PCGExGeo::TConvexHull2();
			TArray<PCGExGeo::TFVtx<2>*> HullVertices;
			GetVerticesFromPoints(Context->CurrentIO->GetIn()->GetPoints(), HullVertices);

			if (Context->ConvexHull->Prepare(HullVertices))
			{
				if (Context->bDoAsyncProcessing) { Context->ConvexHull->StartAsyncProcessing(Context->GetAsyncManager()); }
				else { Context->ConvexHull->Generate(); }
				Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
			}
			else
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Check for singularities."));
				return false;
			}
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if (Context->bDoAsyncProcessing) { Context->ConvexHull->Finalize(); }


		const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();

		if (Settings->bMarkHull && !Settings->bPrunePoints)
		{
			Context->ConvexHull->GetHullIndices(Context->HullIndices);

			PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
			HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

			for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

			HullMarkPointWriter->Write();
			PCGEX_DELETE(HullMarkPointWriter)
		}

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 6);
		if (Settings->bPrunePoints) { Context->GraphBuilder->EnablePointsPruning(); }

		TArray<PCGExGraph::FUnsignedEdge> Edges;
		Context->ConvexHull->GetUniqueEdges(Edges);
		Context->GraphBuilder->Graph->InsertEdges(Edges);

		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->BuildPath();
		Context->GraphBuilder->Write(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		Context->PathsIO->OutputTo(Context);
	}

	return Context->IsDone();
}

void FPCGExBuildConvexHull2DContext::BuildPath()
{
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;

	ConvexHull->GetUniqueEdges(Edges);

	const PCGExData::FPointIO& PathIO = PathsIO->Emplace_GetRef(*CurrentIO, PCGExData::EInit::NewOutput);

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
			MutablePathPoints.Add(CurrentIO->GetInPoint(FirstIndex));
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
		MutablePathPoints.Add(CurrentIO->GetInPoint(CurrentIndex));
	}

	VisitedEdges.Empty();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
