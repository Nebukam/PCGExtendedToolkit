// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildDelaunayGraph2D.h"

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

FPCGExBuildDelaunayGraph2DContext::~FPCGExBuildDelaunayGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE(MainSites)
}

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
		Context->MainSites = new PCGExData::FPointIOCollection(Context);
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildDelaunay2D::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}

				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBuildDelaunay2D::FProcessor>* NewBatch)
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
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();
	if (Context->MainSites)
	{
		Context->MainSites->PruneNullEntries(true);
		Context->MainSites->OutputToContext();
	}

	return Context->TryComplete();
}

namespace PCGExBuildDelaunay2D
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildDelaunay2D::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		ProjectionDetails.Init(Context, PointDataFacade);

		// Build delaunay

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Delaunay = new PCGExGeo::TDelaunay2();

		if (!Delaunay->Process(ActivePositions, ProjectionDetails))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generated invalid results."));
			PCGEX_DELETE(Delaunay)
			return false;
		}

		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::DuplicateInput);

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
			if (Settings->UrquhartSitesMerge != EPCGExUrquhartSiteMergeMode::None) { AsyncManagerPtr->Start<FOutputDelaunayUrquhartSites2D>(BatchIndex, PointIO, this); }
			else { AsyncManagerPtr->Start<FOutputDelaunaySites2D>(BatchIndex, PointIO, this); }
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
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildDelaunayGraph2D)

		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			PCGEX_DELETE(GraphBuilder)
			return;
		}

		GraphBuilder->Write();

		if (HullMarkPointWriter)
		{
			HullMarkPointWriter = PointDataFacade->GetWriter<bool>(Settings->HullAttributeName, false, false, true);
			StartParallelLoopForPoints();
		}
	}

	void FProcessor::Write()
	{
		if (!GraphBuilder) { return; }
		PointDataFacade->Write(AsyncManagerPtr, true);
	}

	bool FOutputDelaunaySites2D::ExecuteTask()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDelaunaySites2D::ExecuteTask);

		FPCGExBuildDelaunayGraph2DContext* Context = Manager->GetContext<FPCGExBuildDelaunayGraph2DContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph2D)

		PCGExData::FPointIO* SitesIO = new PCGExData::FPointIO(Context, PointIO);
		SitesIO->InitializeOutput(PCGExData::EInit::NewOutput);
		
		Context->MainSites->InsertUnsafe(Processor->BatchIndex, SitesIO);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay2* Delaunay = Processor->Delaunay;
		const int32 NumSites = Delaunay->Sites.Num();

		PCGEX_SET_NUM_UNINITIALIZED(MutablePoints, NumSites)
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
			PCGEx::TAttributeWriter<bool>* HullWriter = new PCGEx::TAttributeWriter<bool>(Settings->SiteHullAttributeName);
			HullWriter->BindAndSetNumUninitialized(SitesIO);
			for (int i = 0; i < NumSites; i++) { HullWriter->Values[i] = Delaunay->Sites[i].bOnHull; }
			PCGEX_ASYNC_WRITE_DELETE(Manager, HullWriter);
		}

		return true;
	}

	bool FOutputDelaunayUrquhartSites2D::ExecuteTask()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDelaunayUrquhartSites2D::ExecuteTask);

		FPCGExBuildDelaunayGraph2DContext* Context = Manager->GetContext<FPCGExBuildDelaunayGraph2DContext>();
		PCGEX_SETTINGS(BuildDelaunayGraph2D)

		PCGExData::FPointIO* SitesIO = new PCGExData::FPointIO(Context, PointIO);
		SitesIO->InitializeOutput(PCGExData::EInit::NewOutput);
		
		Context->MainSites->InsertUnsafe(Processor->BatchIndex, SitesIO);

		const TArray<FPCGPoint>& OriginalPoints = SitesIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = SitesIO->GetOut()->GetMutablePoints();
		PCGExGeo::TDelaunay2* Delaunay = Processor->Delaunay;
		const int32 NumSites = Delaunay->Sites.Num();
		PCGEX_SET_NUM_UNINITIALIZED(MutablePoints, NumSites)

		TSet<int32> VisitedSites;
		TArray<bool> Hull;
		TArray<int32> FinalSites;
		FinalSites.Reserve(NumSites);
		Hull.Reserve(NumSites / 4);

		for (int i = 0; i < NumSites; i++)
		{
			bool bAlreadyVisited;
			VisitedSites.Add(i, &bAlreadyVisited);

			if (bAlreadyVisited) { continue; }

			const PCGExGeo::FDelaunaySite2& Site = Delaunay->Sites[i];

			TSet<int32> Queue;
			TSet<uint64> QueuedEdges;
			Delaunay->GetMergedSites(i, Processor->UrquhartEdges, Queue, QueuedEdges);
			VisitedSites.Append(Queue);

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
			PCGEx::TAttributeWriter<bool>* HullWriter = new PCGEx::TAttributeWriter<bool>(Settings->SiteHullAttributeName);
			HullWriter->BindAndSetNumUninitialized(SitesIO);
			for (int i = 0; i < Hull.Num(); i++) { HullWriter->Values[i] = Hull[i]; }
			PCGEX_ASYNC_WRITE_DELETE(Manager, HullWriter);
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
