// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildConvexHull.h"

#include "Data/PCGExData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExConsolidateGraph.h"
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

	PCGEX_DELETE(ClustersIO)

	PCGEX_DELETE(ConvexHull)

	PCGEX_DELETE(EdgeNetwork)
	PCGEX_DELETE(Markings)

	HullIndices.Empty();
	IndicesRemap.Empty();
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

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->ClustersIO = new PCGExData::FPointIOGroup();
	Context->ClustersIO->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;

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
		PCGEX_DELETE(Context->ConvexHull)
		Context->HullIndices.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 3)
			{
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have too few points to be processed (<= 4)."));
				return false;
			}

			Context->ConvexHull = new PCGExGeo::TConvexHull3();
			TArray<PCGExGeo::TFVtx<3>*> HullVertices;
			GetVerticesFromPoints(Context->CurrentIO->GetIn()->GetPoints(), HullVertices);

			if (Context->ConvexHull->Prepare(HullVertices))
			{
				Context->ConvexHull->StartAsyncProcessing(Context->GetAsyncManager());
				Context->SetAsyncState(PCGExGeo::State_ProcessingHull);
			}
			else
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs generates no results. Are points coplanar? If so, use Convex Hull 2D instead."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}
		}
	}

	if (Context->IsState(PCGExGeo::State_ProcessingHull))
	{
		if (Context->IsAsyncWorkComplete())
		{
			Context->ConvexHull->Finalize();
			Context->ConvexHull->GetHullIndices(Context->HullIndices);

			const TArray<FPCGPoint>& InPoints = Context->CurrentIO->GetIn()->GetPoints();
			
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

void FPCGExBuildConvexHullElement::WriteEdges(FPCGExBuildConvexHullContext* Context) const
{
	PCGEX_SETTINGS(BuildConvexHull)

	// Find unique edges
	TSet<uint64> UniqueEdges;
	TArray<PCGExGraph::FUnsignedEdge> Edges;

	if (Settings->bPrunePoints) { Context->ConvexHull->GetUniqueEdgesRemapped(Edges, Context->IndicesRemap); }
	else { Context->ConvexHull->GetUniqueEdges(Edges); }

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

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
