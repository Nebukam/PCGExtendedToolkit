// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierOffset.h"

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"
#include "Data/PCGExData.h"
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


		Settings->Offset.TryReadDataValue(PointDataFacade->Source, OffsetValue);

		PCGExCavalier::FRootPath RootPath(PointDataFacade->Source, ProjectionDetails);

		// Create polyline from root path
		PCGExCavalier::FPolyline Polyline = PCGExCavalier::FContourUtils::CreateFromRootPath(RootPath);

		// Build lookup map for 3D conversion
		RootPaths.Add(RootPath.PathId, MoveTemp(RootPath));

		// Run offset
		TArray<PCGExCavalier::FPolyline> OutputLines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetValue, Settings->OffsetOptions);

		for (PCGExCavalier::FPolyline& Line : OutputLines)
		{
			if (Settings->bTessellateArcs)
			{
				PCGExCavalier::FPolyline TessellatedLine = Line.Tessellated(Settings->TessellationSettings);
				OutputPolyline(TessellatedLine);
			}
			else
			{
				OutputPolyline(Line);
			}
		}

		return true;
	}

	void FProcessor::OutputPolyline(PCGExCavalier::FPolyline& Polyline)
	{
		const int32 OutNumVertices = Polyline.VertexCount();
		if (OutNumVertices < 2) { return; }

		// Convert back to 3D with proper Z and transform interpolation
		PCGExCavalier::FContourResult3D Result3D = PCGExCavalier::FContourUtils::ConvertTo3D(Polyline, RootPaths);

		const TSharedPtr<PCGExData::FPointIO> PathIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
		if (!PathIO) { return; }

		PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), OutNumVertices, EPCGPointNativeProperties::Transform);

		TPCGValueRange<FTransform> OutTransforms = PathIO->GetOut()->GetTransformValueRange();
		for (int32 i = 0; i < OutNumVertices; i++)
		{
			// Full transform with proper Z, rotation, and scale from source
			OutTransforms[i] = ProjectionDetails.Restore(Result3D.Transforms[i], Result3D.GetPointIndex(i));
		}

		PCGExPaths::Helpers::SetClosedLoop(PathIO, Polyline.IsClosed());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
