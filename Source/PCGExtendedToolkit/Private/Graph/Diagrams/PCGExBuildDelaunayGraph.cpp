// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Diagrams/PCGExBuildDelaunayGraph.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph

PCGExData::EIOInit UPCGExBuildDelaunayGraphSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

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
		Context->MainSites = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->MainSites->OutputPin = PCGExGraph::OutputSitesLabel;
		Context->MainSites->Pairs.Init(nullptr, Context->MainPoints->Num());
	}

	return true;
}

bool FPCGExBuildDelaunayGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildDelaunayGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 4 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildDelaunay::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 4)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBuildDelaunay::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid inputs to build from."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();
	if (Context->MainSites)
	{
		Context->MainSites->PruneNullEntries(true);
		Context->MainSites->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExBuildDelaunay
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildDelaunay::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->Source->GetIn()->GetPoints(), ActivePositions);

		Delaunay = MakeUnique<PCGExGeo::TDelaunay3>();

		bool bProcessed = false;
		if (Settings->bMarkHull) { bProcessed = Delaunay->Process<false, true>(ActivePositions); }
		else { bProcessed = Delaunay->Process<false, false>(ActivePositions); }

		if (!bProcessed)
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generated invalid results. Are points coplanar? If so, use Delaunay 2D instead."));
			return false;
		}

		if (!PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::Duplicate)) { return false; }

		if (Settings->bUrquhart)
		{
			if (Settings->bOutputSites && Settings->bMergeUrquhartSites) { Delaunay->RemoveLongestEdges(ActivePositions, UrquhartEdges); }
			else { Delaunay->RemoveLongestEdges(ActivePositions); }
		}

		ActivePositions.Empty();

		PCGEX_SHARED_THIS_DECL
		if (Settings->bOutputSites)
		{
			if (Settings->bMergeUrquhartSites) { PCGEX_LAUNCH(FOutputDelaunayUrquhartSites, PointDataFacade->Source, ThisPtr) }
			else { PCGEX_LAUNCH(FOutputDelaunaySites, PointDataFacade->Source, ThisPtr) }
		}

		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
		GraphBuilder->Graph->InsertEdges(Delaunay->DelaunayEdges, -1);
		GraphBuilder->CompileAsync(AsyncManager, false);

		if (!Settings->bMarkHull && !Settings->bOutputSites) { Delaunay.Reset(); }

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		HullMarkPointWriter->GetMutable(Index) = Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		GraphBuilder->StageEdgesOutputs();

		if (Settings->bMarkHull)
		{
			HullMarkPointWriter = PointDataFacade->GetWritable<bool>(Settings->HullAttributeName, false, true, PCGExData::EBufferInit::New);
			StartParallelLoopForPoints();
		}
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}

	void FOutputDelaunaySites::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExBuildDelaunayGraphContext* Context = AsyncManager->GetContext<FPCGExBuildDelaunayGraphContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph)

		const TSharedPtr<PCGExData::FPointIO> SitesIO = NewPointIO(PointIO.ToSharedRef());
		PCGEX_INIT_IO_VOID(SitesIO, PCGExData::EIOInit::New)

		Context->MainSites->InsertUnsafe(Processor->BatchIndex, SitesIO);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay3* Delaunay = Processor->Delaunay.Get();
		const int32 NumSites = Delaunay->Sites.Num();

		MutablePoints.SetNumUninitialized(NumSites);
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
			PCGEX_MAKE_SHARED(HullBuffer, PCGExData::TBuffer<bool>, SitesIO.ToSharedRef(), Settings->SiteHullAttributeName)
			HullBuffer->PrepareWrite(false, true, PCGExData::EBufferInit::New);
			{
				TArray<bool>& OutValues = *HullBuffer->GetOutValues();
				for (int i = 0; i < NumSites; i++) { OutValues[i] = static_cast<bool>(Delaunay->Sites[i].bOnHull); }
			}
			Write(AsyncManager, HullBuffer);
		}
	}

	void FOutputDelaunayUrquhartSites::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExBuildDelaunayGraphContext* Context = AsyncManager->GetContext<FPCGExBuildDelaunayGraphContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph)

		const TSharedPtr<PCGExData::FPointIO> SitesIO = NewPointIO(PointIO.ToSharedRef());
		PCGEX_INIT_IO_VOID(SitesIO, PCGExData::EIOInit::New)

		Context->MainSites->InsertUnsafe(Processor->BatchIndex, SitesIO);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay3* Delaunay = Processor->Delaunay.Get();
		const int32 NumSites = Delaunay->Sites.Num();

		MutablePoints.SetNumUninitialized(NumSites);
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
			PCGEX_MAKE_SHARED(HullBuffer, PCGExData::TBuffer<bool>, SitesIO.ToSharedRef(), Settings->SiteHullAttributeName)
			HullBuffer->PrepareWrite(false, true, PCGExData::EBufferInit::New);
			{
				TArray<bool>& OutValues = *HullBuffer->GetOutValues();
				for (int i = 0; i < NumSites; i++) { OutValues[i] = static_cast<bool>(Delaunay->Sites[i].bOnHull); }
			}
			Write(AsyncManager, HullBuffer);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
