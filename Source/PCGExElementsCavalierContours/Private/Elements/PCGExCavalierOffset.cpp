// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierOffset.h"

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCUtils.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Math/PCGExBestFitPlane.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierOffsetElement"
#define PCGEX_NAMESPACE CavalierOffset

PCGEX_INITIALIZE_ELEMENT(CavalierOffset)

FPCGExGeo2DProjectionDetails UPCGExCavalierOffsetSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

PCGEX_ELEMENT_BATCH_POINT_IMPL(CavalierOffset)

bool FPCGExCavalierOffsetElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExCavalierProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierOffset)

	return true;
}

bool FPCGExCavalierOffsetElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCavalierOffsetElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CavalierOffset)

	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be affected."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			}, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
			}))
		{
			Context->CancelExecution(TEXT("Could not find any paths to offset."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

bool FPCGExCavalierOffsetElement::WantsRootPathsFromMainInput() const
{
	// Individual FProcessor have their own
	return false;
}

namespace PCGExCavalierOffset
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCavalierOffset::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }


		ProjectionDetails = Settings->ProjectionDetails;
		if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { if (!ProjectionDetails.Init(PointDataFacade)) { return false; } }
		else { ProjectionDetails.Init(PCGExMath::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }


		bool bDual = true;
		Settings->DualOffset.TryReadDataValue(PointDataFacade->Source, bDual);
		Settings->Offset.TryReadDataValue(PointDataFacade->Source, OffsetValue);
		Settings->Iterations.TryReadDataValue(PointDataFacade->Source, NumIterations);

		NumIterations = FMath::Max(1, NumIterations);

		PCGExCavalier::FRootPath RootPath(0, PointDataFacade, ProjectionDetails);

		// Create polyline from root path
		PCGExCavalier::FPolyline Polyline = PCGExCavalier::FContourUtils::CreateFromRootPath(RootPath);
		if (Settings->bAddFuzzinessToPositions) { PCGExCavalier::Utils::AddFuzzinessToPositions(Polyline); }

		// Build lookup map for 3D conversion
		RootPathsMap.Add(RootPath.PathId, MoveTemp(RootPath));

		// Run offset
		for (int i = 0; i < NumIterations; i++)
		{
			TArray<PCGExCavalier::FPolyline> OutputLines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetValue * (i + 1), Settings->OffsetOptions);

			for (PCGExCavalier::FPolyline& Line : OutputLines)
			{
				ProcessOutput(Context->OutputPolyline(Line, false, ProjectionDetails, &RootPathsMap), i, false);
			}
		}

		if (bDual)
		{
			for (int i = 0; i < NumIterations; i++)
			{
				TArray<PCGExCavalier::FPolyline> OutputLines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetValue * -1 * (i + 1), Settings->OffsetOptions);

				for (PCGExCavalier::FPolyline& Line : OutputLines)
				{
					ProcessOutput(Context->OutputPolyline(Line, false, ProjectionDetails, &RootPathsMap), i, true);
				}
			}
		}

		return true;
	}

	void FProcessor::ProcessOutput(const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Iteration, const bool bDual) const
	{
		if (!IO) { return; }

		// Write iteration attribute
		if (Settings->bWriteIteration) { PCGExData::Helpers::SetDataValue(IO->GetOut(), Settings->IterationAttributeName, Iteration); }

		// Tag with iteration number
		if (Settings->bTagIteration) { IO->Tags->Set(Settings->IterationTag, Iteration); }

		// Tag dual offsets
		if (Settings->bTagDual && bDual) { IO->Tags->AddRaw(Settings->DualTag); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
