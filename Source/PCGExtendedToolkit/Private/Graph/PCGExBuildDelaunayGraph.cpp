// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph

PCGExData::EInit UPCGExBuildDelaunayGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExBuildDelaunayGraphContext::~FPCGExBuildDelaunayGraphContext()
{
	PCGEX_TERMINATE_ASYNC
	SitesIOMap.Empty();
	PCGEX_DELETE(MainSites)
}

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	if (bOutputSites) { PCGEX_PIN_POINTS(PCGExGraph::OutputSitesLabel, "Complete delaunay sites.", Required, {}) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph)

bool FPCGExBuildDelaunayGraphElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph)
	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	if (Settings->bOutputSites)
	{
		if (Settings->bMarkSiteHull) { PCGEX_VALIDATE_NAME(Settings->SiteHullAttributeName) }
		Context->MainSites = new PCGExData::FPointIOCollection(Context);
		Context->MainSites->DefaultOutputLabel = PCGExGraph::OutputSitesLabel;
	}

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

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildDelaunay::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 4)
				{
					bInvalidInputs = true;
					return false;
				}

				if (Context->MainSites)
				{
					PCGExData::FPointIO* SitesIO = Context->MainSites->Emplace_GetRef(Entry, PCGExData::EInit::NoOutput);
					Context->SitesIOMap.Add(Entry, SitesIO);
				}

				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBuildDelaunay::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any points to build from."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 4 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();
	if (Context->MainSites) { Context->MainSites->OutputToContext(); }

	return Context->TryComplete();
}

namespace PCGExBuildDelaunay
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Delaunay)

		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(HullMarkPointWriter)

		UrquhartEdges.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildDelaunay::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildDelaunayGraph)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Delaunay = new PCGExGeo::TDelaunay3();

		if (!Delaunay->Process(ActivePositions, false))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generated invalid results. Are points coplanar? If so, use Delaunay 2D instead."));
			PCGEX_DELETE(Delaunay)
			return false;
		}

		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::DuplicateInput);

		if (Settings->bUrquhart)
		{
			if (Settings->bOutputSites && Settings->bMergeUrquhartSites) { Delaunay->RemoveLongestEdges(ActivePositions, UrquhartEdges); }
			else { Delaunay->RemoveLongestEdges(ActivePositions); }
		}

		if (Settings->bMarkHull) { HullMarkPointWriter = new PCGEx::TAttributeWriter<bool>(Settings->HullAttributeName, false, false); }

		ActivePositions.Empty();

		if (Settings->bOutputSites)
		{
			if (Settings->bMergeUrquhartSites) { AsyncManagerPtr->Start<FOutputDelaunayUrquhartSites>(BatchIndex, PointIO, this); }
			else { AsyncManagerPtr->Start<FOutputDelaunaySites>(BatchIndex, PointIO, this); }
		}

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderDetails);
		GraphBuilder->Graph->InsertEdges(Delaunay->DelaunayEdges, -1);
		GraphBuilder->CompileAsync(AsyncManagerPtr);

		if (!Settings->bMarkHull && !Settings->bOutputSites) { PCGEX_DELETE(Delaunay) }

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		HullMarkPointWriter->Values[Index] = Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			PCGEX_DELETE(GraphBuilder)
			PCGEX_DELETE(HullMarkPointWriter)
			return;
		}

		GraphBuilder->Write();

		if (HullMarkPointWriter)
		{
			HullMarkPointWriter->BindAndSetNumUninitialized(PointIO);
			StartParallelLoopForPoints();
		}
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder) { return; }
		if (HullMarkPointWriter) { HullMarkPointWriter->Write(); }
	}

	bool FOutputDelaunaySites::ExecuteTask()
	{
		FPCGExBuildDelaunayGraphContext* Context = Manager->GetContext<FPCGExBuildDelaunayGraphContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph)

		PCGExData::FPointIO* SitesIO = Context->SitesIOMap[PointIO];
		SitesIO->InitializeOutput(PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay3* Delaunay = Processor->Delaunay;
		const int32 NumSites = Delaunay->Sites.Num();

		PCGEX_SET_NUM_UNINITIALIZED(MutablePoints, NumSites)
		for (int i = 0; i < NumSites; i++)
		{
			const PCGExGeo::FDelaunaySite3& Site = Delaunay->Sites[i];

			FVector Centroid = OriginalPoints[Site.Vtx[0]].Transform.GetLocation();
			Centroid += OriginalPoints[Site.Vtx[1]].Transform.GetLocation();
			Centroid += OriginalPoints[Site.Vtx[2]].Transform.GetLocation();
			Centroid += OriginalPoints[Site.Vtx[3]].Transform.GetLocation();
			Centroid /= 4;

			MutablePoints[i] = OriginalPoints[Site.Vtx[0]];
			MutablePoints[i].Transform.SetLocation(Centroid);
		}

		if (Settings->bMarkSiteHull)
		{
			PCGEx::TAttributeWriter<bool>* HullWriter = new PCGEx::TAttributeWriter<bool>(Settings->SiteHullAttributeName);
			HullWriter->BindAndSetNumUninitialized(SitesIO);
			for (int i = 0; i < NumSites; i++) { HullWriter->Values[i] = Delaunay->Sites[i].bOnHull; }
			PCGEX_ASYNC_WRITE_DELETE(Manager, HullWriter);
		}

		return true;
	}

	bool FOutputDelaunayUrquhartSites::ExecuteTask()
	{
		FPCGExBuildDelaunayGraphContext* Context = Manager->GetContext<FPCGExBuildDelaunayGraphContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph)

		PCGExData::FPointIO* SitesIO = Context->SitesIOMap[PointIO];
		SitesIO->InitializeOutput(PCGExData::EInit::NewOutput);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay3* Delaunay = Processor->Delaunay;
		const int32 NumSites = Delaunay->Sites.Num();

		PCGEX_SET_NUM_UNINITIALIZED(MutablePoints, NumSites)
		for (int i = 0; i < NumSites; i++)
		{
			const PCGExGeo::FDelaunaySite3& Site = Delaunay->Sites[i];

			FVector Centroid = OriginalPoints[Site.Vtx[0]].Transform.GetLocation();
			Centroid += OriginalPoints[Site.Vtx[1]].Transform.GetLocation();
			Centroid += OriginalPoints[Site.Vtx[2]].Transform.GetLocation();
			Centroid += OriginalPoints[Site.Vtx[3]].Transform.GetLocation();
			Centroid /= 4;

			MutablePoints[i] = OriginalPoints[Site.Vtx[0]];
			MutablePoints[i].Transform.SetLocation(Centroid);
		}

		if (Settings->bMarkSiteHull)
		{
			PCGEx::TAttributeWriter<bool>* HullWriter = new PCGEx::TAttributeWriter<bool>(Settings->SiteHullAttributeName);
			HullWriter->BindAndSetNumUninitialized(SitesIO);
			for (int i = 0; i < NumSites; i++) { HullWriter->Values[i] = Delaunay->Sites[i].bOnHull; }
			PCGEX_ASYNC_WRITE_DELETE(Manager, HullWriter);
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
