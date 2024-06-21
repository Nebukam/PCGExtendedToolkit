// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildVoronoiGraph.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph

namespace PCGExGeoTask
{
	class FLloydRelax3;
}

PCGExData::EInit UPCGExBuildVoronoiGraphSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExBuildVoronoiGraphContext::~FPCGExBuildVoronoiGraphContext()
{
	PCGEX_TERMINATE_ASYNC
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExBuildVoronoiGraphSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph)

bool FPCGExBuildVoronoiGraphElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

	return true;
}

bool FPCGExBuildVoronoiGraphElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBuildVoronoi::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 4)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBuildVoronoi::FProcessor>* NewBatch)
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
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 4 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExBuildVoronoi
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Voronoi)

		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(HullMarkPointWriter)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildVoronoiGraph)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Voronoi = new PCGExGeo::TVoronoi3();

		if (!Voronoi->Process(ActivePositions))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generated invalid results. Are points coplanar? If so, use Voronoi 2D instead."));
			PCGEX_DELETE(Voronoi)
			return false;
		}

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);

		TArray<FPCGPoint>& Centroids = PointIO->GetOut()->GetMutablePoints();
		const int32 NumSites = Voronoi->Centroids.Num();
		Centroids.SetNum(NumSites);

		if (Settings->Method == EPCGExCellCenter::Circumcenter)
		{
			for (int i = 0; i < NumSites; i++) { Centroids[i].Transform.SetLocation(Voronoi->Circumspheres[i].Center); }
		}
		else if (Settings->Method == EPCGExCellCenter::Centroid)
		{
			for (int i = 0; i < NumSites; i++) { Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]); }
		}
		else if (Settings->Method == EPCGExCellCenter::Balanced)
		{
			const FBox Bounds = PointIO->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);
			for (int i = 0; i < NumSites; i++)
			{
				FVector Target = Voronoi->Circumspheres[i].Center;
				if (Bounds.IsInside(Target)) { Centroids[i].Transform.SetLocation(Target); }
				else { Centroids[i].Transform.SetLocation(Voronoi->Centroids[i]); }
			}
		}

		/*
		if (Settings->bMarkHull)
		{
			HullMarkPointWriter = new PCGEx::TFAttributeWriter<bool>(Settings->HullAttributeName, false, false);
			HullMarkPointWriter->BindAndSetNumUninitialized(*PointIO);
			StartParallelLoopForRange(PointIO->GetNum());
		}
		else
		{
			PCGEX_DELETE(Voronoi)
		}
		*/

		ActivePositions.Empty();

		GraphBuilder = new PCGExGraph::FGraphBuilder(PointIO, &Settings->GraphBuilderSettings);
		GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);

		GraphBuilder->CompileAsync(AsyncManagerPtr);

		PCGEX_DELETE(Voronoi)

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point)
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

		GraphBuilder->Write(Context);
		if (HullMarkPointWriter) { HullMarkPointWriter->Write(); }
	}

	void FProcessor::Write()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
