// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplineToPath.h"

#include "Sampling/PCGExSampleNearestSpline.h"

#define LOCTEXT_NAMESPACE "PCGExSplineToPathElement"
#define PCGEX_NAMESPACE SplineToPath

PCGEX_INITIALIZE_ELEMENT(SplineToPath)

TArray<FPCGPinProperties> UPCGExSplineToPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = TArray<FPCGPinProperties>();
	PCGEX_PIN_POLYLINES(PCGEx::SourceTargetsLabel, "The spline datas to convert to paths.", Required, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSplineToPathSettings::OutputPinProperties() const
{
	return Super::OutputPinProperties();
}

bool FPCGExSplineToPathElement::Boot(FPCGExContext* InContext) const
{
	// Do not boot normally, as we care only about spline inputs

	PCGEX_CONTEXT_AND_SETTINGS(SplineToPath)

	if (Context->InputData.GetInputs().IsEmpty()) { return false; } //Get rid of errors and warning when there is no input

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);

	if (!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData) { continue; }

			switch (Settings->SampleInputs)
			{
			default:
			case EPCGExSplineSamplingIncludeMode::All:
				Context->Targets.Add(SplineData);
				break;
			case EPCGExSplineSamplingIncludeMode::ClosedLoopOnly:
				if (SplineData->SplineStruct.bClosedLoop) { Context->Targets.Add(SplineData); }
				break;
			case EPCGExSplineSamplingIncludeMode::OpenSplineOnly:
				if (!SplineData->SplineStruct.bClosedLoop) { Context->Targets.Add(SplineData); }
				break;
			}
		}

		Context->NumTargets = Context->Targets.Num();
	}

	if (Context->NumTargets <= 0)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No targets (either no input or empty dataset)"));
		return false;
	}

	Context->Splines.Reserve(Context->NumTargets);
	for (const UPCGSplineData* SplineData : Context->Targets) { Context->Splines.Add(SplineData->SplineStruct); }

	PCGEX_FOREACH_FIELD_SPLINETOPATH(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSplineToPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSplineToPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SplineToPath)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		for (int i = 0; i < Context->NumTargets; ++i)
		{
			TSharedPtr<PCGExData::FPointIO> NewOutput = Context->MainPoints->Emplace_GetRef(PCGExData::EInit::NewOutput);
			TSharedPtr<PCGExData::FFacade> PointDataFacade = MakeShared<PCGExData::FFacade>(NewOutput.ToSharedRef());
			Context->GetAsyncManager()->Start<PCGExSplineToPath::FWriteTask>(i, NewOutput, PointDataFacade);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_ASYNC_WAIT
		Context->Done();
	}

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSplineToPath
{
	bool FWriteTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (!PointDataFacade) { return false; }

		FPCGExSplineToPathContext* Context = AsyncManager->GetContext<FPCGExSplineToPathContext>();
		check(Context);

		const UPCGSplineData* SplineData = Context->Targets[TaskIndex];
		const FPCGSplineStruct& Spline = Context->Splines[TaskIndex];

		const int32 NumSegments = Spline.GetNumberOfSplineSegments();
		const double Length = Spline.GetSplineLength();

		TArray<FPCGPoint> MutablePoints = PointDataFacade->Source->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(NumSegments + 1);

		const FInterpCurveVector& Positions = Spline.GetSplinePointsPosition();


		for (int i = 0; i < NumSegments; ++i)
		{
			const int32 StartIndex = i;
			const int32 EndIndex = i + 1;
			FPCGPoint& Point = MutablePoints[StartIndex];
			//Point.Transform.SetLocation(Positions.Points[StartIndex].);
		}

		PointDataFacade->Write(AsyncManager);

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
