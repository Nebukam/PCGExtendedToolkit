// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Diagrams/PCGExBuildDelaunayGraph2D.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildDelaunayGraph2D

namespace PCGExGeoTask
{
	class FLloydRelax2;
}

PCGExData::EInit UPCGExBuildDelaunayGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExBuildDelaunayGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	if (bOutputSites) { PCGEX_PIN_POINTS(PCGExGraph::OutputSitesLabel, "Complete delaunay sites.", Required, {}) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildDelaunayGraph2D)

bool FPCGExBuildDelaunayGraph2DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	if (Settings->bOutputSites)
	{
		if (Settings->bMarkSiteHull) { PCGEX_VALIDATE_NAME(Settings->SiteHullAttributeName) }
		Context->MainSites = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->MainSites->DefaultOutputLabel = PCGExGraph::OutputSitesLabel;
		Context->MainSites->Pairs.Init(nullptr, Context->MainPoints->Pairs.Num());
	}

	return true;
}

bool FPCGExBuildDelaunayGraph2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildDelaunayGraph2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildDelaunay2D::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}

				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBuildDelaunay2D::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to build from."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
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

namespace PCGExBuildDelaunay2D
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildDelaunay2D::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		ProjectionDetails.Init(ExecutionContext, PointDataFacade);

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->Source->GetIn()->GetPoints(), ActivePositions);

		Delaunay = MakeUnique<PCGExGeo::TDelaunay2>();

		if (!Delaunay->Process(ActivePositions, ProjectionDetails))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generated invalid results."));
			return false;
		}

		PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(Context, PCGExData::EInit::DuplicateInput);

		if (Settings->bUrquhart)
		{
			if (Settings->bOutputSites && Settings->UrquhartSitesMerge != EPCGExUrquhartSiteMergeMode::None)
			{
				Delaunay->RemoveLongestEdges(ActivePositions, UrquhartEdges);
			}
			else { Delaunay->RemoveLongestEdges(ActivePositions); }
		}

		ActivePositions.Empty();

		if (Settings->bOutputSites)
		{
			if (Settings->UrquhartSitesMerge != EPCGExUrquhartSiteMergeMode::None) { AsyncManager->Start<FOutputDelaunayUrquhartSites2D>(BatchIndex, PointDataFacade->Source, SharedThis(this)); }
			else { AsyncManager->Start<FOutputDelaunaySites2D>(BatchIndex, PointDataFacade->Source, SharedThis(this)); }
		}

		GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
		GraphBuilder->Graph->InsertEdges(Delaunay->DelaunayEdges, -1);
		GraphBuilder->CompileAsync(AsyncManager, false);

		if (!Settings->bMarkHull && !Settings->bOutputSites) { Delaunay.Reset(); }

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		HullMarkPointWriter->GetMutable(Index) = Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			return;
		}

		GraphBuilder->OutputEdgesToContext();

		if (HullMarkPointWriter) // BUG
		{
			HullMarkPointWriter = PointDataFacade->GetWritable<bool>(Settings->HullAttributeName, false, false, true);
			StartParallelLoopForPoints();
		}
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}

	bool FOutputDelaunaySites2D::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDelaunaySites2D::ExecuteTask);

		FPCGExBuildDelaunayGraph2DContext* Context = AsyncManager->GetContext<FPCGExBuildDelaunayGraph2DContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph2D)

		const TSharedPtr<PCGExData::FPointIO> SitesIO = MakeShared<PCGExData::FPointIO>(Context, PointIO.ToSharedRef());
		SitesIO->InitializeOutput(Context, PCGExData::EInit::NewOutput);

		Context->MainSites->InsertUnsafe(Processor->BatchIndex, SitesIO);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay2* Delaunay = Processor->Delaunay.Get();
		const int32 NumSites = Delaunay->Sites.Num();

		MutablePoints.SetNumUninitialized(NumSites);
		for (int i = 0; i < NumSites; i++)
		{
			const PCGExGeo::FDelaunaySite2& Site = Delaunay->Sites[i];

			FVector Centroid = FVector::ZeroVector;
			for (int j = 0; j < 3; j++) { Centroid += (OriginalPoints.GetData() + Site.Vtx[j])->Transform.GetLocation(); }
			Centroid /= 3;

			MutablePoints[i] = *(OriginalPoints.GetData() + Site.Vtx[0]);
			MutablePoints[i].Transform.SetLocation(Centroid);
		}

		if (Settings->bMarkSiteHull)
		{
			const TSharedPtr<PCGExData::TBuffer<bool>> HullBuffer = MakeShared<PCGExData::TBuffer<bool>>(SitesIO.ToSharedRef(), Settings->SiteHullAttributeName);
			HullBuffer->PrepareWrite(false, true, true);
			{
				TArray<bool>& OutValues = *HullBuffer->GetOutValues();
				for (int i = 0; i < NumSites; i++) { OutValues[i] = Delaunay->Sites[i].bOnHull; }
			}
			Write(AsyncManager, HullBuffer);
		}

		return true;
	}

	bool FOutputDelaunayUrquhartSites2D::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDelaunayUrquhartSites2D::ExecuteTask);

		FPCGExBuildDelaunayGraph2DContext* Context = AsyncManager->GetContext<FPCGExBuildDelaunayGraph2DContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph2D)

		TSharedPtr<PCGExData::FPointIO> SitesIO = MakeShared<PCGExData::FPointIO>(Context, PointIO.ToSharedRef());
		SitesIO->InitializeOutput(Context, PCGExData::EInit::NewOutput);

		Context->MainSites->InsertUnsafe(Processor->BatchIndex, SitesIO);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay2* Delaunay = Processor->Delaunay.Get();
		const int32 NumSites = Delaunay->Sites.Num();

		MutablePoints.SetNumUninitialized(NumSites);

		TBitArray<> VisitedSites;
		VisitedSites.Init(false, NumSites);

		TArray<bool> Hull;
		TArray<int32> FinalSites;
		FinalSites.Reserve(NumSites);
		Hull.Reserve(NumSites / 4);

		for (int i = 0; i < NumSites; i++)
		{
			if (VisitedSites[i]) { continue; }
			VisitedSites[i] = true;

			const PCGExGeo::FDelaunaySite2& Site = Delaunay->Sites[i];

			TSet<int32> QueueSet;
			TSet<uint64> QueuedEdgesSet;
			Delaunay->GetMergedSites(i, Processor->UrquhartEdges, QueueSet, QueuedEdgesSet, VisitedSites);

			TArray<int32> Queue = QueueSet.Array();
			QueueSet.Empty();

			TArray<uint64> QueuedEdges = QueuedEdgesSet.Array();
			QueuedEdgesSet.Empty();

			FVector Centroid = FVector::ZeroVector;
			bool bOnHull = Site.bOnHull;

			if (Settings->UrquhartSitesMerge == EPCGExUrquhartSiteMergeMode::MergeSites)
			{
				for (const int32 MergeSiteIndex : Queue)
				{
					const PCGExGeo::FDelaunaySite2& MSite = Delaunay->Sites[MergeSiteIndex];
					for (int j = 0; j < 3; j++) { Centroid += (OriginalPoints.GetData() + MSite.Vtx[j])->Transform.GetLocation(); }

					if (!bOnHull && Settings->bMarkSiteHull && MSite.bOnHull) { bOnHull = true; }
				}

				Centroid /= (Queue.Num() * 3);
			}
			else
			{
				if (Settings->bMarkSiteHull)
				{
					for (const int32 MergeSiteIndex : Queue)
					{
						if (!bOnHull && Delaunay->Sites[MergeSiteIndex].bOnHull)
						{
							bOnHull = true;
							break;
						}
					}
				}

				for (const uint64 EdgeHash : QueuedEdges)
				{
					Centroid += FMath::Lerp(
						(OriginalPoints.GetData() + PCGEx::H64A(EdgeHash))->Transform.GetLocation(),
						(OriginalPoints.GetData() + PCGEx::H64B(EdgeHash))->Transform.GetLocation(), 0.5);
				}

				Centroid /= (QueuedEdges.Num());
			}

			const int32 VIndex = FinalSites.Add(Site.Vtx[0]);

			Hull.Add(bOnHull);
			MutablePoints[VIndex] = OriginalPoints[Site.Vtx[0]];
			MutablePoints[VIndex].Transform.SetLocation(Centroid);
		}

		MutablePoints.SetNum(FinalSites.Num());

		if (Settings->bMarkSiteHull)
		{
			const TSharedPtr<PCGExData::TBuffer<bool>> HullBuffer = MakeShared<PCGExData::TBuffer<bool>>(SitesIO.ToSharedRef(), Settings->SiteHullAttributeName);
			HullBuffer->PrepareWrite(false, true, true);
			{
				TArray<bool>& OutValues = *HullBuffer->GetOutValues();
				for (int i = 0; i < Hull.Num(); i++) { OutValues[i] = Hull[i]; }
			}
			Write(AsyncManager, HullBuffer);
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
