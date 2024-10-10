// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Diagrams/PCGExBuildVoronoiGraph.h"

#include "PCGExRandom.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph

namespace PCGExGeoTask
{
	class FLloydRelax3;
}

PCGExData::EInit UPCGExBuildVoronoiGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	//PCGEX_PIN_POINTS(PCGExGraph::OutputSitesLabel, "Complete delaunay sites.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph)

bool FPCGExBuildVoronoiGraphElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	Context->SitesOutput = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->SitesOutput->DefaultOutputLabel = PCGExGraph::OutputSitesLabel;

	return true;
}

bool FPCGExBuildVoronoiGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildVoronoi::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 4)
				{
					bInvalidInputs = true;
					return false;
				}

				Context->SitesOutput->Emplace_GetRef(Entry, PCGExData::EInit::NewOutput);
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBuildVoronoi::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to build from."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 4 points and won't be processed."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();
	//Context->SitesOutput->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExBuildVoronoi
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildVoronoi::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->Source->GetIn()->GetPoints(), ActivePositions);

		Voronoi = MakeUnique<PCGExGeo::TVoronoi3>();

		/*
		auto ExtractValidSites = [&]()
		{
			const PCGExData::FPointIO* SitesIO = Context->SitesOutput->Pairs[BatchIndex];
			const TArray<FPCGPoint>& OriginalSites = PointIO->GetIn()->GetPoints();
			TArray<FPCGPoint>& MutableSites = SitesIO->GetOut()->GetMutablePoints();
			for (int i = 0; i < OriginalSites.Num(); i++)
			{
				if (Voronoi->Delaunay->DelaunayHull.Contains(i)) { continue; }
				MutableSites.Add(OriginalSites[i]);
			}
		};
		*/

		if (!Voronoi->Process(ActivePositions))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generated invalid results. Are points coplanar? If so, use Voronoi 2D instead."));
			return false;
		}

		ActivePositions.Empty();

		PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(Context, PCGExData::EInit::NewOutput);
		const FBox Bounds = PointDataFacade->Source->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);

		if (Settings->Method == EPCGExCellCenter::Circumcenter && Settings->bPruneOutOfBounds)
		{
			TArray<FPCGPoint>& Centroids = PointDataFacade->GetOut()->GetMutablePoints();

			const int32 NumSites = Voronoi->Centroids.Num();
			TArray<int32> RemappedIndices;
			RemappedIndices.SetNumUninitialized(NumSites);
			Centroids.Reserve(NumSites);

			for (int i = 0; i < NumSites; i++)
			{
				const FVector Centroid = Voronoi->Circumspheres[i].Center;
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
			Voronoi.Reset();

			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(ValidEdges, -1);

			ValidEdges.Empty();
		}
		else
		{
			TArray<FPCGPoint>& Centroids = PointDataFacade->GetOut()->GetMutablePoints();
			const int32 NumSites = Voronoi->Centroids.Num();
			Centroids.SetNum(NumSites);

			if (Settings->Method == EPCGExCellCenter::Circumcenter)
			{
				for (int i = 0; i < NumSites; i++)
				{
					Centroids[i].Transform.SetLocation(Voronoi->Circumspheres[i].Center);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Centroid)
			{
				for (int i = 0; i < NumSites; i++)
				{
					Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]);
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Balanced)
			{
				for (int i = 0; i < NumSites; i++)
				{
					FVector Target = Voronoi->Circumspheres[i].Center;
					if (Bounds.IsInside(Target)) { Centroids[i].Transform.SetLocation(Target); }
					else { Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]); }
					Centroids[i].Seed = PCGExRandom::ComputeSeed(Centroids[i]);
				}
			}

			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);

			//ExtractValidSites();
			Voronoi.Reset();
		}

		GraphBuilder->CompileAsync(AsyncManager, false);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		//HullMarkPointWriter->Values[Index] = Voronoi->Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NoOutput);
			return;
		}

		GraphBuilder->OutputEdgesToContext();
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
