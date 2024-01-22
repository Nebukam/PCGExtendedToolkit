// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateCustomGraph.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull

int32 UPCGExBuildConvexHullSettings::GetPreferredChunkSize() const { return 32; }

PCGExData::EInit UPCGExBuildConvexHullSettings::GetMainOutputInitMode() const
{
	return bPrunePoints ? PCGExData::EInit::NewOutput : bMarkHull ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::Forward;
}

FPCGExBuildConvexHullContext::~FPCGExBuildConvexHullContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(GraphBuilder)
	PCGEX_DELETE(ConvexHull)

	HullIndices.Empty();
}

TArray<FPCGPinProperties> UPCGExBuildConvexHullSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinClustersOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinClustersOutput.Tooltip = FTEXT("Point data representing edges.");
#endif


	return PinProperties;
}

FName UPCGExBuildConvexHullSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull)

bool FPCGExBuildConvexHullElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)

	if (!Settings->bPrunePoints) { PCGEX_VALIDATE_NAME(Settings->HullAttributeName) }

	Context->GraphBuilderSettings.bPruneIsolatedPoints = Settings->bPrunePoints;

	return true;
}

bool FPCGExBuildConvexHullElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHullElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull)

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
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 4)."));
				return false;
			}

			Context->ConvexHull = new PCGExGeo::TConvexHull3();
			TArray<PCGExGeo::TFVtx<3>*> HullVertices;
			GetVerticesFromPoints(Context->CurrentIO->GetIn()->GetPoints(), HullVertices);

			if (Context->ConvexHull->Prepare(HullVertices))
			{
				if (Context->bDoAsyncProcessing) { Context->ConvexHull->StartAsyncProcessing(Context->GetAsyncManager()); }
				else { Context->ConvexHull->Generate(); }
				Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
			}
			else
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(1) Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));
				return false;
			}
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		if (Context->bDoAsyncProcessing) { Context->ConvexHull->Finalize(); }
		Context->ConvexHull->GetHullIndices(Context->HullIndices);

		if (Settings->bMarkHull && !Settings->bPrunePoints)
		{
			PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
			HullMarkPointWriter->BindAndGet(*Context->CurrentIO);

			for (int i = 0; i < Context->CurrentIO->GetNum(); i++) { HullMarkPointWriter->Values[i] = Context->HullIndices.Contains(i); }

			HullMarkPointWriter->Write();
			PCGEX_DELETE(HullMarkPointWriter)
		}

		Context->GraphBuilder = new PCGExGraph::FGraphBuilder(*Context->CurrentIO, &Context->GraphBuilderSettings, 6);

		TArray<PCGExGraph::FUnsignedEdge> Edges;
		Context->ConvexHull->GetUniqueEdges(Edges);
		Context->GraphBuilder->Graph->InsertEdges(Edges);

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

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
