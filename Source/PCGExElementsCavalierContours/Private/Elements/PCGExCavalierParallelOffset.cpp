// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierParallelOffset.h"

#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCShapeOffset.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Math/PCGExBestFitPlane.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierParallelOffsetElement"
#define PCGEX_NAMESPACE CavalierParallelOffset

PCGEX_INITIALIZE_ELEMENT(CavalierParallelOffset)

FPCGExGeo2DProjectionDetails UPCGExCavalierParallelOffsetSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool FPCGExCavalierParallelOffsetElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExCavalierProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierParallelOffset)

	return true;
}

bool FPCGExCavalierParallelOffsetElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCavalierParallelOffsetElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CavalierParallelOffset)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bDual = true;
		double OffsetValue = 10.0;
		int32 NumIterations = 1;

		// Read settings from first input if available
		if (!Context->MainPoints->IsEmpty())
		{
			const TSharedPtr<PCGExData::FPointIO>& FirstIO = (*Context->MainPoints)[0];
			Settings->DualOffset.TryReadDataValue(FirstIO, bDual);
			Settings->Offset.TryReadDataValue(FirstIO, OffsetValue);
			Settings->Iterations.TryReadDataValue(FirstIO, NumIterations);
		}

		NumIterations = FMath::Max(1, NumIterations);

		// Build shape offset options
		PCGExCavalier::ShapeOffset::FShapeOffsetOptions ShapeOptions;
		ShapeOptions.PosEqualEps = Settings->OffsetOptions.PositionEqualEpsilon;
		ShapeOptions.OffsetDistEps = Settings->OffsetOptions.OffsetDistanceEpsilon;
		ShapeOptions.SliceJoinEps = Settings->OffsetOptions.SliceJoinEpsilon;

		// Process positive offsets
		for (int32 Iteration = 0; Iteration < NumIterations; ++Iteration)
		{
			const double CurrentOffset = OffsetValue * (Iteration + 1);

			// Perform shape parallel offset
			TArray<PCGExCavalier::FPolyline> OffsetResults = PCGExCavalier::ShapeOffset::ParallelOffsetShape(
				Context->MainPolylines,
				CurrentOffset,
				ShapeOptions);

			// Output results
			for (PCGExCavalier::FPolyline& ResultPline : OffsetResults)
			{
				const bool bIsHole = ResultPline.Area() < 0.0;
				ProcessOutput(
					Context, Settings,
					Context->OutputPolyline(ResultPline, bIsHole, Context->ProjectionDetails),
					Iteration, false, bIsHole);
			}
		}

		// Process dual (negative) offsets if enabled
		if (bDual)
		{
			for (int32 Iteration = 0; Iteration < NumIterations; ++Iteration)
			{
				const double CurrentOffset = -OffsetValue * (Iteration + 1);

				TArray<PCGExCavalier::FPolyline> OffsetResults = PCGExCavalier::ShapeOffset::ParallelOffsetShape(
					Context->MainPolylines,
					CurrentOffset,
					ShapeOptions);

				for (PCGExCavalier::FPolyline& ResultPline : OffsetResults)
				{
					const bool bIsHole = ResultPline.Area() < 0.0;
					ProcessOutput(
						Context, Settings,
						Context->OutputPolyline(ResultPline, bIsHole, Context->ProjectionDetails),
						Iteration, true, bIsHole);
				}
			}
		}

		Context->Done();
	}

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

void FPCGExCavalierParallelOffsetElement::ProcessOutput(
	FPCGExCavalierParallelOffsetContext* Context,
	const UPCGExCavalierParallelOffsetSettings* Settings,
	const TSharedPtr<PCGExData::FPointIO>& IO,
	int32 Iteration,
	bool bIsDual,
	bool bIsHole) const
{
	if (!IO) return;

	// Write iteration attribute
	if (Settings->bWriteIteration) { PCGExData::Helpers::SetDataValue(IO->GetOut(), Settings->IterationAttributeName, Iteration); }

	// Tag with iteration number
	if (Settings->bTagIteration) { IO->Tags->Set(Settings->IterationTag, Iteration); }

	// Tag dual offsets
	if (Settings->bTagDual && bIsDual) { IO->Tags->AddRaw(Settings->DualTag); }

	// Tag based on orientation
	if (Settings->bTagOrientation)
	{
		if (bIsHole) { IO->Tags->AddRaw(Settings->HoleTag); }
		else { IO->Tags->AddRaw(Settings->OuterTag); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
