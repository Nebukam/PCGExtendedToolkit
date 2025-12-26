// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierOffset.h"

#include "Core/PCGExCCOffset.h"
#include "Core/PCGExCCPolyline.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExArrayHelpers.h"
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

		Settings->Offset.TryReadDataValue(PointDataFacade->Source, OffsetValue);

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();
		const int32 InNumVertices = InTransforms.Num();

		const bool bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(PointDataFacade->Source);

		TArray<PCGExCavalier::FInputPoint> InputPoints;
		InputPoints.Reserve(InNumVertices);

		for (const FTransform& InTransform : InTransforms)
		{
			InputPoints.Emplace(InTransform);
		}

		PCGExCavalier::FPolyline Polyline = PCGExCavalier::FPolyline::FromInputPoints(InputPoints, bClosedLoop);
		TArray<PCGExCavalier::FPolyline> OutputLines = PCGExCavalier::Offset::ParallelOffset(Polyline, OffsetValue, Settings->OffsetOptions);

		for (PCGExCavalier::FPolyline& Line : OutputLines)
		{
			const int32 OutNumVertices = Line.GetVertices().Num();

			if (OutNumVertices < 2) { continue; }

			TSharedPtr<PCGExData::FPointIO> PathIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source, PCGExData::EIOInit::New);
			PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), OutNumVertices, EPCGPointNativeProperties::Transform);

			TPCGValueRange<FTransform> OutTransforms = PathIO->GetOut()->GetTransformValueRange();
			for (int i = 0; i < OutNumVertices; i++)
			{
				FTransform& OutTransform = OutTransforms[i];
				OutTransform.SetLocation(FVector(Line.GetVertices()[i].X, Line.GetVertices()[i].Y, 0));
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
