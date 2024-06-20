// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExBuildVoronoiGraph2D.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Geometry/PCGExGeoDelaunay.h"
#include "Geometry/PCGExGeoVoronoi.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildVoronoiGraph2D

PCGExData::EInit UPCGExBuildVoronoiGraph2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBuildVoronoiGraph2DContext::~FPCGExBuildVoronoiGraph2DContext()
{
	PCGEX_TERMINATE_ASYNC
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	return PinProperties;
}

FName UPCGExBuildVoronoiGraph2DSettings::GetMainOutputLabel() const { return PCGExGraph::OutputVerticesLabel; }

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph2D)

bool FPCGExBuildVoronoiGraph2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)

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

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

namespace PCGExBuildVoronoi2D
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Voronoi)

		PCGEX_DELETE(GraphBuilder)
		PCGEX_DELETE(HullMarkPointWriter)
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		ProjectionSettings = Settings->ProjectionSettings;
		ProjectionSettings.Init(PointIO);

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		Voronoi = new PCGExGeo::TVoronoi2();

		if (!Voronoi->Process(ActivePositions, ProjectionSettings))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some inputs generated invalid results."));
			PCGEX_DELETE(Voronoi)
			return false;
		}

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);

		TArray<FPCGPoint>& Centroids = PointIO->GetOut()->GetMutablePoints();
		const int32 NumSites = Voronoi->Centroids.Num();
		Centroids.SetNum(NumSites);

		if (Settings->Method == EPCGExCellCenter::Circumcenter)
		{
			for (int i = 0; i < NumSites; i++) { Centroids[i].Transform.SetLocation(Voronoi->Circumcenters[i]); }
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
				FVector Target = Voronoi->Circumcenters[i];
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

		GraphBuilder = new PCGExGraph::FGraphBuilder(*PointIO, &Settings->GraphBuilderSettings);
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
