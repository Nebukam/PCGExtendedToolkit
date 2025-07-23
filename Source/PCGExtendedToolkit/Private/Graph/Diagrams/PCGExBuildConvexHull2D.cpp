// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Diagrams/PCGExBuildConvexHull2D.h"


#include "Curve/CurveUtil.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Math/ConvexHull2d.h"
#include "Graph/PCGExCluster.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExGraph"
#define PCGEX_NAMESPACE BuildConvexHull2D

TArray<FPCGPinProperties> UPCGExBuildConvexHull2DSettings::OutputPinProperties() const
{
	if (!bOutputClusters)
	{
		TArray<FPCGPinProperties> PinProperties;
		PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Point data representing closed convex hull paths.", Required, {})
		return PinProperties;
	}
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExGraph::OutputEdgesLabel, "Point data representing edges.", Required, {})
	PCGEX_PIN_POINTS(PCGExPaths::OutputPathsLabel, "Point data representing closed convex hull paths.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildConvexHull2D)

bool FPCGExBuildConvexHull2DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)

	Context->PathsIO = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->PathsIO->OutputPin = PCGExPaths::OutputPathsLabel;

	return true;
}

bool FPCGExBuildConvexHull2DElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildConvexHull2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildConvexHull2D)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 3 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExConvexHull2D::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 3)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExConvexHull2D::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid inputs to build from."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	if (Settings->bOutputClusters)
	{
		Context->MainPoints->StageOutputs();
		Context->MainBatch->Output(); // Edges in order
	}

	Context->PathsIO->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExConvexHull2D
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExConvexHull2D::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		ProjectionDetails = Settings->ProjectionDetails;
		if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { if (!ProjectionDetails.Init(ExecutionContext, PointDataFacade)) { return false; } }
		else { ProjectionDetails.Init(PCGExGeo::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }

		// Build convex hull

		TArray<FVector> ActivePositions;
		PCGExGeo::PointsToPositions(PointDataFacade->Source->GetIn(), ActivePositions);
		ProjectionDetails.Project(ActivePositions, ActivePositions);

		TArray<int32> ConvexHullIndices;
		ConvexHull2D::ComputeConvexHull(ActivePositions, ConvexHullIndices);

		const int32 LastIndex = ConvexHullIndices.Num() - 1;
		if (LastIndex < 0)
		{
			// Not enough points to build a convex hull
			return false;
		}

		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->PathsIO->Emplace_GetRef(PointDataFacade->GetIn(), PCGExData::EIOInit::New);
		if (!PathIO) { return false; }

		PathIO->IOIndex = PointDataFacade->Source->IOIndex;
		UPCGBasePointData* MutablePoints = PathIO->GetOut();
		TArray<FVector2D> ProjectedPoints;

		(void)PCGEx::SetNumPointsAllocated(MutablePoints, LastIndex + 1, PointDataFacade->GetAllocations());
		ProjectedPoints.Reserve(LastIndex + 1);

		PCGExPaths::SetClosedLoop(PathIO->GetOut(), true);

		for (int i = 0; i <= LastIndex; i++) { ProjectedPoints.Emplace(ActivePositions[ConvexHullIndices[i]]); }
		if (!PCGExGeo::IsWinded(Settings->Winding, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(ProjectedPoints) < 0)) { Algo::Reverse(ConvexHullIndices); }

		if (Settings->bOutputClusters)
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

			GraphBuilder = MakeShared<PCGExGraph::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);

			PCGExGraph::FEdge E;
			for (int i = 0; i <= LastIndex; i++)
			{
				const int32 CurrentIndex = ConvexHullIndices[i];
				const int32 NextIndex = ConvexHullIndices[i == LastIndex ? 0 : i + 1];

				ProjectedPoints.Emplace(ActivePositions[CurrentIndex]);
				GraphBuilder->Graph->InsertEdge(CurrentIndex, NextIndex, E);
			}

			if (!PCGExGeo::IsWinded(Settings->Winding, UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(ProjectedPoints) < 0))
			{
				Algo::Reverse(ConvexHullIndices);
			}

			PointDataFacade->Source->InheritPoints(ConvexHullIndices, 0);
			PathIO->InheritPoints(ConvexHullIndices, 0);

			ActivePositions.Empty();

			GraphBuilder->CompileAsync(AsyncManager, true);
		}
		else
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)
			PathIO->InheritPoints(ConvexHullIndices, 0);
			ActivePositions.Empty();
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder || !GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
		}
	}

	void FProcessor::Output()
	{
		if (!Settings->bOutputClusters) { return; }
		GraphBuilder->StageEdgesOutputs();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
