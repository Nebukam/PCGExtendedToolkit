// Copyright 2025 Timothé Lapetite and contributors
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
	Context->SitesOutput->OutputPin = PCGExGraph::OutputSitesLabel;

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
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 4 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildVoronoi::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 4)
				{
					bHasInvalidInputs = true;
					return false;
				}

				Context->SitesOutput->Emplace_GetRef(Entry, PCGExData::EIOInit::New);
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBuildVoronoi::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid inputs to build from."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();
	//Context->SitesOutput->OutputToContext();
	Context->MainBatch->Output();

	return Context->TryComplete();
}

namespace PCGExBuildVoronoi
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildVoronoi::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->Source->GetIn(), ActivePositions);

		Voronoi = MakeUnique<PCGExGeo::TVoronoi3>();

		if (!Voronoi->Process(ActivePositions))
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generated invalid results. Are points coplanar? If so, use Voronoi 2D instead."));
			return false;
		}

		ActivePositions.Empty();

		if (!PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New)) { return false; }
		const FBox Bounds = PointDataFacade->Source->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);

		if (Settings->Method == EPCGExCellCenter::Circumcenter && Settings->bPruneOutOfBounds)
		{
			int32 Centroids = 0;

			const int32 NumSites = Voronoi->Centroids.Num();
			TArray<int32> RemappedIndices;
			RemappedIndices.SetNumUninitialized(NumSites);

			for (int i = 0; i < NumSites; i++)
			{
				const FVector Centroid = Voronoi->Circumspheres[i].Center;
				if (!Bounds.IsInside(Centroid))
				{
					RemappedIndices[i] = -1;
					continue;
				}

				RemappedIndices[i] = Centroids++;
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

			UPCGBasePointData* CentroidsPoints = PointDataFacade->GetOut();
			(void)PCGEx::SetNumPointsAllocated(CentroidsPoints, Centroids, PointDataFacade->GetAllocations());

			TPCGValueRange<FTransform> OutTransforms = CentroidsPoints->GetTransformValueRange(true);

			for (int i = 0; i < RemappedIndices.Num(); i++)
			{
				if (const int32 Idx = RemappedIndices[i]; Idx != -1) { OutTransforms[Idx].SetLocation(Voronoi->Circumspheres[i].Center); }
			}

			RemappedIndices.Empty();

			Voronoi.Reset();

			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(ValidEdges, -1);

			ValidEdges.Empty();
		}
		else
		{
			UPCGBasePointData* Centroids = PointDataFacade->GetOut();
			const int32 NumSites = Voronoi->Centroids.Num();
			(void)PCGEx::SetNumPointsAllocated(Centroids, NumSites, PointDataFacade->GetAllocations());

			TPCGValueRange<FTransform> OutTransforms = Centroids->GetTransformValueRange(false);

			if (Settings->Method == EPCGExCellCenter::Circumcenter)
			{
				for (int i = 0; i < NumSites; i++)
				{
					OutTransforms[i].SetLocation(Voronoi->Circumspheres[i].Center);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Centroid)
			{
				for (int i = 0; i < NumSites; i++)
				{
					OutTransforms[i].SetLocation(Voronoi->Centroids[i]);
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Balanced)
			{
				for (int i = 0; i < NumSites; i++)
				{
					FVector Target = Voronoi->Circumspheres[i].Center;
					if (Bounds.IsInside(Target)) { OutTransforms[i].SetLocation(Target); }
					else { OutTransforms[i].SetLocation(Voronoi->Centroids[i]); }
				}
			}

			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);

			//ExtractValidSites();
			Voronoi.Reset();
		}

		// Update seeds

		const int32 NumSites = PointDataFacade->GetOut()->GetNumPoints();
		TPCGValueRange<FTransform> OutTransforms = PointDataFacade->GetOut()->GetTransformValueRange(false);
		TPCGValueRange<int32> OutSeeds = PointDataFacade->GetOut()->GetSeedValueRange(false);

		for (int i = 0; i < NumSites; i++)
		{
			OutSeeds[i] = PCGExRandom::ComputeSpatialSeed(OutTransforms[i].GetLocation());
		}

		// Compile graph

		GraphBuilder->bInheritNodeData = false; // We're creating new points from scratch, we don't want the inheritance.
		GraphBuilder->CompileAsync(AsyncManager, false);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		//HullMarkPointWriter->Values[Index] = Voronoi->Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
		}
	}

	void FProcessor::Write()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}

	void FProcessor::Output()
	{
		GraphBuilder->StageEdgesOutputs();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
