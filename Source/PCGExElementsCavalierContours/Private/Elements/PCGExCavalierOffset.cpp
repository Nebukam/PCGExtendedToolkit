// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierOffset.h"

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierOffsetElement"
#define PCGEX_NAMESPACE CavalierOffset

PCGEX_INITIALIZE_ELEMENT(CavalierOffset)

PCGEX_ELEMENT_BATCH_POINT_IMPL(CavalierOffset)

bool FPCGExCavalierOffsetElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

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

		PCGExCavalier::FRootPath RootPath(PointDataFacade->Source, ProjectionDetails);

		// Create polyline from root path
		PCGExCavalier::FPolyline Polyline = PCGExCavalier::FContourUtils::CreateFromRootPath(RootPath, Settings->bAddFuzzinessToPositions);

		// Build lookup map for 3D conversion
		RootPaths.Add(RootPath.PathId, MoveTemp(RootPath));

		// Run offset
		for (int i = 0; i < NumIterations; i++)
		{
			TArray<PCGExCavalier::FPolyline> OutputLines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetValue * (i + 1), Settings->OffsetOptions);

			for (PCGExCavalier::FPolyline& Line : OutputLines)
			{
				TSharedPtr<PCGExData::FPointIO> IO = nullptr;
				if (Settings->bTessellateArcs)
				{
					PCGExCavalier::FPolyline TessellatedLine = Line.Tessellated(Settings->TessellationSettings);
					IO = OutputPolyline(TessellatedLine);
				}
				else
				{
					IO = OutputPolyline(Line);
				}

				ProcessOutput(IO, i, false);
			}
		}

		if (bDual)
		{
			for (int i = 0; i < NumIterations; i++)
			{
				TArray<PCGExCavalier::FPolyline> OutputLines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetValue * -1 * (i + 1), Settings->OffsetOptions);

				for (PCGExCavalier::FPolyline& Line : OutputLines)
				{
					TSharedPtr<PCGExData::FPointIO> IO = nullptr;
					if (Settings->bTessellateArcs)
					{
						PCGExCavalier::FPolyline TessellatedLine = Line.Tessellated(Settings->TessellationSettings);
						IO = OutputPolyline(TessellatedLine);
					}
					else
					{
						IO = OutputPolyline(Line);
					}

					ProcessOutput(IO, i, true);
				}
			}
		}

		return true;
	}

	TSharedPtr<PCGExData::FPointIO> FProcessor::OutputPolyline(PCGExCavalier::FPolyline& Polyline)
	{
		const int32 OutNumVertices = Polyline.VertexCount();
		if (OutNumVertices < 2) { return nullptr; }

		// Convert back to 3D with proper Z and transform interpolation
		PCGExCavalier::FContourResult3D Result3D = PCGExCavalier::FContourUtils::ConvertTo3D(Polyline, RootPaths, Settings->bBlendTransforms);

		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
		if (!PathIO) { return nullptr; }

		EPCGPointNativeProperties Allocations = PointDataFacade->GetAllocations() | EPCGPointNativeProperties::Transform;
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), OutNumVertices, Allocations);
		TArray<int32>& IdxMapping = PathIO->GetIdxMapping(OutNumVertices);

		TPCGValueRange<FTransform> OutTransforms = PathIO->GetOut()->GetTransformValueRange();
		int32 LastValidIndex = -1;
		for (int32 i = 0; i < OutNumVertices; i++)
		{
			const int32 OriginalIndex = Result3D.GetPointIndex(i);
			if (OriginalIndex != INDEX_NONE)
			{
				LastValidIndex = OriginalIndex;
				IdxMapping[i] = OriginalIndex;
			}
			else
			{
				IdxMapping[i] = LastValidIndex;
			}

			// Full transform with proper Z, rotation, and scale from source
			OutTransforms[i] = ProjectionDetails.Restore(Result3D.Transforms[i], OriginalIndex);
		}

		EnumRemoveFlags(Allocations, EPCGPointNativeProperties::Transform);
		PathIO->ConsumeIdxMapping(Allocations);

		PCGExPaths::Helpers::SetClosedLoop(PathIO, Polyline.IsClosed());
		return PathIO;
	}

	void FProcessor::ProcessOutput(const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Iteration, const bool bDual) const
	{
		if (!IO) { return; }
		if (Settings->bWriteIteration) { PCGExData::Helpers::SetDataValue(IO->GetOut(), Settings->IterationAttributeName, Iteration); }
		if (Settings->bTagIteration) { IO->Tags->Set(Settings->IterationTag, Iteration); }
		if (Settings->bTagDual && bDual) { IO->Tags->AddRaw(Settings->DualTag); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
