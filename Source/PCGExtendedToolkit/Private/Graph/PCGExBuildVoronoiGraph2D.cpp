// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildVoronoiGraph2D.h"

#include "PCGExRandom.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph2D

PCGExData::EInit UPCGExBuildVoronoiGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBuildVoronoiGraph2DContext::~FPCGExBuildVoronoiGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(SitesOutput)
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	//PCGEX_PIN_POINTS(PCGExGraph::OutputSitesLabel, "Complete delaunay sites.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph2D)

bool FPCGExBuildVoronoiGraph2DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->SitesOutput = new PCGExData::FPointIOCollection(Context);
	Context->SitesOutput->DefaultOutputLabel = PCGExGraph::OutputSitesLabel;

	return true;
}

bool FPCGExBuildVoronoiGraph2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraph2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildVoronoi2D::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bInvalidInputs = true;
					return false;
				}

				Context->SitesOutput->Emplace_GetRef(Entry, PCGExData::EInit::NewOutput);
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBuildVoronoi2D::FProcessor>* NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
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
	//Context->SitesOutput->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExBuildVoronoi2D
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Voronoi)

		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(HullMarkPointWriter)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildVoronoi2D::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		ProjectionDetails.Init(Context, PointDataFacade);

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Voronoi = new PCGExGeo::TVoronoi2();

		/*
		auto ExtractValidSites = [&]()
		{
			const PCGExData::FPointIO* SitesIO = TypedContext->SitesOutput->Pairs[BatchIndex];
			const TArray<FPCGPoint>& OriginalSites = PointIO->GetIn()->GetPoints();
			TArray<FPCGPoint>& MutableSites = SitesIO->GetOut()->GetMutablePoints();
			for (int i = 0; i < OriginalSites.Num(); ++i)
			{
				if (Voronoi->Delaunay->DelaunayHull.Contains(i)) { continue; }
				MutableSites.Add(OriginalSites[i]);
			}
		};
		*/

		if (!Voronoi->Process(ActivePositions, ProjectionDetails))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generated invalid results."));
			PCGEX_DELETE(Voronoi)
			return false;
		}

		ActivePositions.Empty();

		PointIO->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EInit::NewOutput);

		if (Settings->Method == EPCGExCellCenter::Circumcenter && Settings->bPruneOutOfBounds)
		{
			const FBox Bounds = PointIO->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);
			TArray<FPCGPoint>& Centroids = PointIO->GetOut()->GetMutablePoints();

			int32 NumSites = Voronoi->Centroids.Num();
			TArray<int32> RemappedIndices;
			RemappedIndices.SetNumUninitialized(NumSites);
			Centroids.Reserve(NumSites);

			for (int i = 0; i < NumSites; ++i)
			{
				const FVector Centroid = Voronoi->Circumcenters[i];
				if (!Bounds.IsInside(Centroid))
				{
					RemappedIndices[i] = -1;
					continue;
				}

				RemappedIndices[i] = Centroids.Num();
				FPCGPoint& NewPoint = Centroids.Emplace_GetRef();
				NewPoint.Transform.SetLocation(Centroid);
				NewPoint.Seed = PCGExRandom::ComputeSeed(NewPoint);
			}

			TArray<uint64> ValidEdges;
			ValidEdges.Reserve(Voronoi->VoronoiEdges.Num());

			for (const uint64 Hash : Voronoi->VoronoiEdges)
			{
				const int32 A = RemappedIndices[PCGEx::H64A(Hash)];
				const int32 B = RemappedIndices[PCGEx::H64B(Hash)];
				if (A == -1 || B == -1) { continue; }
				ValidEdges.Add(PCGEx::H64(A, B));
			}

			RemappedIndices.Empty();
			//ExtractValidSites();
			PCGEX_DELETE(Voronoi)

			GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(ValidEdges, -1);

			ValidEdges.Empty();
		}
		else
		{
			TArray<FPCGPoint>& Centroids = PointIO->GetOut()->GetMutablePoints();
			const int32 NumSites = Voronoi->Centroids.Num();
			Centroids.SetNum(NumSites);

			if (Settings->Method == EPCGExCellCenter::Circumcenter)
			{
				for (int i = 0; i < NumSites; ++i)
				{
					Centroids[i].Transform.SetLocation(Voronoi->Circumcenters[i]);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Centroid)
			{
				for (int i = 0; i < NumSites; ++i)
				{
					Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Balanced)
			{
				const FBox Bounds = PointIO->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);
				for (int i = 0; i < NumSites; ++i)
				{
					FVector Target = Voronoi->Circumcenters[i];
					if (Bounds.IsInside(Target)) { Centroids[i].Transform.SetLocation(Target); }
					else { Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]); }
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}

			GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);

			//ExtractValidSites();
			PCGEX_DELETE(Voronoi)
		}

		GraphBuilder->CompileAsync(AsyncManagerPtr);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		//HullMarkPointWriter->Values[Index] = Voronoi->Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder) { return; }

		if (!GraphBuilder->bCompiledSuccessfully)
		{
			PointIO->InitializeOutput(PCGExData::EInit::NoOutput);
			return;
		}

		GraphBuilder->Write();
		if (HullMarkPointWriter) { HullMarkPointWriter->Write(); }
	}

	void FProcessor::Write()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
