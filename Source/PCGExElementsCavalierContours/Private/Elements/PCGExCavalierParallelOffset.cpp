// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierParallelOffset.h"

#include "Core/PCGExCCPolyline.h"
#include "Core/PCGExCCShapeOffset.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierParallelOffsetElement"
#define PCGEX_NAMESPACE CavalierParallelOffset

PCGEX_INITIALIZE_ELEMENT(CavalierParallelOffset)

bool FPCGExCavalierParallelOffsetElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierParallelOffset)

	// Initialize projection
	Context->ProjectionDetails = Settings->ProjectionDetails;

	// Build polylines from all inputs
	BuildPolylinesFromCollection(Context, Settings);

	if (Context->InputPolylines.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid closed paths found in input."));
		return false;
	}

	return true;
}

void FPCGExCavalierParallelOffsetElement::BuildPolylinesFromCollection(
	FPCGExCavalierParallelOffsetContext* Context,
	const UPCGExCavalierParallelOffsetSettings* Settings) const
{
	const int32 NumInputs = Context->MainPoints->Num();
	if (NumInputs == 0) return;

	// Thread-safe containers for parallel building
	struct FBuildResult
	{
		PCGExCavalier::FRootPath RootPath;
		PCGExCavalier::FPolyline Polyline;
		TSharedPtr<PCGExData::FPointIO> SourceIO;
		bool bValid = false;
	};

	TArray<FBuildResult> Results;
	Results.SetNum(NumInputs);

	{
		PCGExAsyncHelpers::FAsyncExecutionScope BuildScope(NumInputs);

		for (int32 i = 0; i < NumInputs; ++i)
		{
			const TSharedPtr<PCGExData::FPointIO>& IO = (*Context->MainPoints)[i];

			BuildScope.Execute([&, IO, i]()
			{
				FBuildResult& Result = Results[i];

				// Skip paths with insufficient points
				if (IO->GetNum() < 3) return;

				// Check if closed (required for shape offset)
				const bool bIsClosed = PCGExPaths::Helpers::GetClosedLoop(IO->GetIn());
				if (!bIsClosed && Settings->bSkipOpenPaths) return;

				// Allocate unique path ID
				const int32 PathId = Context->AllocatePathId();

				// Initialize projection for this path
				FPCGExGeo2DProjectionDetails LocalProjection = Context->ProjectionDetails;
				if (LocalProjection.Method == EPCGExProjectionMethod::Normal)
				{
					TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
					if (!LocalProjection.Init(Facade)) { return; }
				}
				else
				{
					LocalProjection.Init(PCGExMath::FBestFitPlane(IO->GetIn()->GetConstTransformValueRange()));
				}

				// Build root path
				Result.RootPath = PCGExCavalier::FRootPath(PathId, true);
				Result.RootPath.Reserve(IO->GetNum());

				TConstPCGValueRange<FTransform> Transforms = IO->GetIn()->GetConstTransformValueRange();
				for (int32 j = 0; j < IO->GetNum(); ++j)
				{
					Result.RootPath.AddPoint(LocalProjection.Project(Transforms[j], j));
				}

				// Convert to polyline
				Result.Polyline = PCGExCavalier::FContourUtils::CreateFromRootPath(Result.RootPath, Settings->bAddFuzzinessToPositions);
				Result.Polyline.SetClosed(true);

				Result.SourceIO = IO;
				Result.bValid = true;
			});
		}
	}

	// Collect results (single-threaded)
	Context->InputPolylines.Reserve(NumInputs);
	Context->InputPathIds.Reserve(NumInputs);

	for (FBuildResult& Result : Results)
	{
		if (!Result.bValid) continue;

		const int32 PathId = Result.RootPath.PathId;
		Context->RootPaths.Add(PathId, MoveTemp(Result.RootPath));
		Context->InputPolylines.Add(MoveTemp(Result.Polyline));
		Context->InputPathIds.Add(PathId);
		Context->SourceIOs.Add(PathId, Result.SourceIO);
	}
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
				Context->InputPolylines,
				Context->InputPathIds,
				CurrentOffset,
				ShapeOptions);

			// Output results
			for (PCGExCavalier::FPolyline& ResultPline : OffsetResults)
			{
				const bool bIsHole = ResultPline.Area() < 0.0;

				if (Settings->bTessellateArcs)
				{
					PCGExCavalier::FPolyline Tessellated = ResultPline.Tessellated(Settings->TessellationSettings);
					TSharedPtr<PCGExData::FPointIO> IO = OutputPolyline(Context, Settings, Tessellated);
					ProcessOutput(Context, Settings, IO, Iteration, false, bIsHole);
				}
				else
				{
					TSharedPtr<PCGExData::FPointIO> IO = OutputPolyline(Context, Settings, ResultPline);
					ProcessOutput(Context, Settings, IO, Iteration, false, bIsHole);
				}
			}
		}

		// Process dual (negative) offsets if enabled
		if (bDual)
		{
			for (int32 Iteration = 0; Iteration < NumIterations; ++Iteration)
			{
				const double CurrentOffset = -OffsetValue * (Iteration + 1);

				TArray<PCGExCavalier::FPolyline> OffsetResults = PCGExCavalier::ShapeOffset::ParallelOffsetShape(
					Context->InputPolylines,
					Context->InputPathIds,
					CurrentOffset,
					ShapeOptions);

				for (PCGExCavalier::FPolyline& ResultPline : OffsetResults)
				{
					const bool bIsHole = ResultPline.Area() < 0.0;

					if (Settings->bTessellateArcs)
					{
						PCGExCavalier::FPolyline Tessellated = ResultPline.Tessellated(Settings->TessellationSettings);
						TSharedPtr<PCGExData::FPointIO> IO = OutputPolyline(Context, Settings, Tessellated);
						ProcessOutput(Context, Settings, IO, Iteration, true, bIsHole);
					}
					else
					{
						TSharedPtr<PCGExData::FPointIO> IO = OutputPolyline(Context, Settings, ResultPline);
						ProcessOutput(Context, Settings, IO, Iteration, true, bIsHole);
					}
				}
			}
		}

		Context->Done();
	}

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

TSharedPtr<PCGExData::FPointIO> FPCGExCavalierParallelOffsetElement::OutputPolyline(
	FPCGExCavalierParallelOffsetContext* Context,
	const UPCGExCavalierParallelOffsetSettings* Settings,
	PCGExCavalier::FPolyline& Polyline) const
{
	const int32 NumVertices = Polyline.VertexCount();
	if (NumVertices < 2) return nullptr;

	// Convert back to 3D using source tracking
	PCGExCavalier::FContourResult3D Result3D = PCGExCavalier::FContourUtils::ConvertTo3D(
		Polyline, Context->RootPaths, Settings->bBlendTransforms);

	// Find a source IO for metadata inheritance
	TSharedPtr<PCGExData::FPointIO> SourceIO = FindSourceIO(Context, Polyline);

	// Create output
	const TSharedPtr<PCGExData::FPointIO> PathIO = Context->MainPoints->Emplace_GetRef(
		SourceIO, PCGExData::EIOInit::New);

	if (!PathIO) return nullptr;

	EPCGPointNativeProperties Allocations = EPCGPointNativeProperties::Transform;
	if (SourceIO)
	{
		const TSharedPtr<PCGExData::FFacade> SourceFacade = MakeShared<PCGExData::FFacade>(SourceIO.ToSharedRef());
		Allocations |= SourceFacade->GetAllocations();
	}

	PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), NumVertices, Allocations);
	TArray<int32>& IdxMapping = PathIO->GetIdxMapping(NumVertices);

	TPCGValueRange<FTransform> OutTransforms = PathIO->GetOut()->GetTransformValueRange();
	int32 LastValidIndex = -1;

	for (int32 i = 0; i < NumVertices; ++i)
	{
		const int32 OriginalIndex = Result3D.GetPointIndex(i);
		if (OriginalIndex != INDEX_NONE)
		{
			LastValidIndex = OriginalIndex;
			IdxMapping[i] = OriginalIndex;
		}
		else
		{
			IdxMapping[i] = FMath::Max(0, LastValidIndex);
		}

		// Full transform with proper Z, rotation, and scale from source
		OutTransforms[i] = Context->ProjectionDetails.Restore(Result3D.Transforms[i], OriginalIndex);
	}

	EnumRemoveFlags(Allocations, EPCGPointNativeProperties::Transform);
	//PathIO->ConsumeIdxMapping(Allocations);

	PCGExPaths::Helpers::SetClosedLoop(PathIO, Polyline.IsClosed());

	return PathIO;
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
	if (Settings->bWriteIteration)
	{
		PCGExData::Helpers::SetDataValue(IO->GetOut(), Settings->IterationAttributeName, Iteration);
	}

	// Tag with iteration number
	if (Settings->bTagIteration)
	{
		IO->Tags->Set(Settings->IterationTag, Iteration);
	}

	// Tag dual offsets
	if (Settings->bTagDual && bIsDual)
	{
		IO->Tags->AddRaw(Settings->DualTag);
	}

	// Tag based on orientation
	if (Settings->bTagOrientation)
	{
		if (bIsHole)
		{
			IO->Tags->AddRaw(Settings->HoleTag);
		}
		else
		{
			IO->Tags->AddRaw(Settings->OuterTag);
		}
	}
}

TSharedPtr<PCGExData::FPointIO> FPCGExCavalierParallelOffsetElement::FindSourceIO(
	FPCGExCavalierParallelOffsetContext* Context,
	const PCGExCavalier::FPolyline& Polyline) const
{
	// Try primary path ID first
	const int32 PrimaryPathId = Polyline.GetPrimaryPathId();
	if (PrimaryPathId != INDEX_NONE)
	{
		if (TSharedPtr<PCGExData::FPointIO>* Found = Context->SourceIOs.Find(PrimaryPathId))
		{
			return *Found;
		}
	}

	// Try contributing paths
	for (int32 PathId : Polyline.GetContributingPathIds())
	{
		if (TSharedPtr<PCGExData::FPointIO>* Found = Context->SourceIOs.Find(PathId))
		{
			return *Found;
		}
	}

	// Fallback: use first source if available
	if (!Context->SourceIOs.IsEmpty())
	{
		for (const auto& Pair : Context->SourceIOs)
		{
			return Pair.Value;
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE