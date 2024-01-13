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

PCGExData::EInit UPCGExBuildDelaunayGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBuildDelaunayGraphContext::~FPCGExBuildDelaunayGraphContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(Delaunay)
	PCGEX_DELETE(ConvexHull)

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
		PCGEX_DELETE(Context->GraphBuilder)
		PCGEX_DELETE(Context->Delaunay)
		PCGEX_DELETE(Context->ConvexHull)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 4)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT(" (0) Some inputs have too few points to be processed (<= 4)."));
				return false;
			}

			if (Settings->bMarkHull)
			{
				Context->ConvexHull = new PCGExGeo::TConvexHull3();
				TArray<PCGExGeo::TFVtx<3>*> HullVertices;
				GetVerticesFromPoints(Context->CurrentIO->GetIn()->GetPoints(), HullVertices);

				if (Context->ConvexHull->Prepare(HullVertices))
				{
					if (Context->bDoAsyncProcessing) { Context->ConvexHull->StartAsyncProcessing(Context->GetAsyncManager()); }
					else { Context->ConvexHull->Generate(); }
				}
				else
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Are points coplanar? If so, use Delaunay 2D instead."));
					return false;
				}
			}

			Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		if (Settings->bMarkHull)
		{
			if (!Context->IsAsyncWorkComplete()) { return false; }

			if (Context->bDoAsyncProcessing) { Context->ConvexHull->Finalize(); }
			Context->ConvexHull->GetHullIndices(Context->HullIndices);

			PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
			HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

			for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

			HullMarkPointWriter->Write();
			PCGEX_DELETE(HullMarkPointWriter)
			PCGEX_DELETE(Context->ConvexHull)
		}

		Context->Delaunay = new PCGExGeo::TDelaunayTriangulation3();
		if (Context->Delaunay->PrepareFrom(Context->CurrentIO->GetIn()->GetPoints()))
		{
			if (Context->bDoAsyncProcessing) { Context->Delaunay->Hull->StartAsyncProcessing(Context->GetAsyncManager()); }
			else { Context->Delaunay->Hull->Generate(); }
			Context->SetAsyncState(PCGExGeo::State_ProcessingDelaunayHull);
		}
		else
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			PCGE_LOG(Warning, GraphAndLog, FTEXT("(2) Some inputs generates no results. Are points coplanar? If so, use Delaunay 2D instead."));
			return false;
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunayHull))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if (Context->bDoAsyncProcessing)
		{
			Context->SetState(PCGExGeo::State_ProcessingDelaunayPreprocess);
		}
		else
		{
			Context->Delaunay->Generate();
			Context->SetAsyncState(PCGExGeo::State_ProcessingDelaunay);
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunayPreprocess))
	{
		auto PreprocessSimplex = [&](const int32 Index) { Context->Delaunay->PreprocessSimplex(Index); };
		if (!Context->Process(PreprocessSimplex, Context->Delaunay->Hull->Simplices.Num())) { return false; }

		Context->Delaunay->Cells.SetNumUninitialized(Context->Delaunay->NumFinalCells);
		Context->SetState(PCGExGeo::State_ProcessingDelaunay);
	}

	if (Context->IsState(PCGExGeo::State_ProcessingDelaunay))
	{
		auto ProcessSimplex = [&](const int32 Index) { Context->Delaunay->ProcessSimplex(Index); };
		if (!Context->Process(ProcessSimplex, Context->Delaunay->NumFinalCells)) { return false; }

		if (Context->Delaunay->Cells.IsEmpty())
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			PCGE_LOG(Warning, GraphAndLog, FTEXT("(3) Some inputs generates no results. Are points coplanar? If so, use Delaunay 2D instead."));
			return false;
		}

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, 8);

		TArray<PCGExGraph::FUnsignedEdge> Edges;
		Context->Delaunay->GetUniqueEdges(Edges);
		Context->GraphBuilder->Graph->InsertEdges(Edges);

		Context->GraphBuilder->Compile(Context);
		Context->SetAsyncState(PCGExGraph::State_WritingClusters);
	}

	if (Context->IsState(PCGExGraph::State_WritingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->GraphBuilder->Write(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
