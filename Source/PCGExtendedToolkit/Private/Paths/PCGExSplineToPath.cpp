// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSplineToPath.h"


#include "Sampling/PCGExSampleNearestSpline.h"

#define LOCTEXT_NAMESPACE "PCGExSplineToPathElement"
#define PCGEX_NAMESPACE SplineToPath

PCGEX_INITIALIZE_ELEMENT(SplineToPath)

TArray<FPCGPinProperties> UPCGExSplineToPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = TArray<FPCGPinProperties>();
	PCGEX_PIN_POLYLINES(PCGExSplineToPath::SourceSplineLabel, "The splines to convert to paths.", Required, {})
	return PinProperties;
}

bool FPCGExSplineToPathElement::Boot(FPCGExContext* InContext) const
{
	// Do not boot normally, as we care only about spline inputs

	PCGEX_CONTEXT_AND_SETTINGS(SplineToPath)

	if (Context->InputData.GetInputs().IsEmpty()) { return false; } //Get rid of errors and warning when there is no input

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGExSplineToPath::SourceSplineLabel);
	PCGEX_FWD(TagForwarding)
	Context->TagForwarding.Init();

	Context->MainPoints = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->MainPoints->OutputPin = Settings->GetMainOutputPin();

	auto AddTags = [&](const TSet<FString>& SourceTags)
	{
		TArray<FString> Tags = SourceTags.Array();
		Context->TagForwarding.Prune(Tags);
		Context->Tags.Add(Tags);
	};

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
				AddTags(TaggedData.Tags);
				break;
			case EPCGExSplineSamplingIncludeMode::ClosedLoopOnly:
				if (SplineData->SplineStruct.bClosedLoop)
				{
					Context->Targets.Add(SplineData);
					AddTags(TaggedData.Tags);
				}
				break;
			case EPCGExSplineSamplingIncludeMode::OpenSplineOnly:
				if (!SplineData->SplineStruct.bClosedLoop)
				{
					Context->Targets.Add(SplineData);
					AddTags(TaggedData.Tags);
				}
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
	PCGEX_ON_INITIAL_EXECUTION
	{
		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		for (int i = 0; i < Context->NumTargets; i++)
		{
			TSharedPtr<PCGExData::FPointIO> NewOutput = Context->MainPoints->Emplace_GetRef(PCGExData::EIOInit::New);
			PCGEX_MAKE_SHARED(PointDataFacade, PCGExData::FFacade, NewOutput.ToSharedRef())
			PCGEX_LAUNCH(PCGExSplineToPath::FWriteTask, i, PointDataFacade)

			NewOutput->Tags->Append(Context->Tags[i]);
		}

		Context->SetAsyncState(PCGEx::State_WaitingOnAsyncWork);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_WaitingOnAsyncWork)
	{
		Context->MainPoints->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

namespace PCGExSplineToPath
{
	void FWriteTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExSplineToPathContext* Context = AsyncManager->GetContext<FPCGExSplineToPathContext>();
		PCGEX_SETTINGS(SplineToPath)

		const UPCGSplineData* SplineData = Context->Targets[TaskIndex];
		check(SplineData)
		const FPCGSplineStruct& Spline = Context->Splines[TaskIndex];

		const int32 NumSegments = Spline.GetNumberOfSplineSegments();
		const double TotalLength = Spline.GetSplineLength();

		TArray<FPCGPoint>& MutablePoints = PointDataFacade->Source->GetOut()->GetMutablePoints();
		const int32 LastIndex = Spline.bClosedLoop ? NumSegments - 1 : NumSegments;
		MutablePoints.SetNum(Spline.bClosedLoop ? NumSegments : NumSegments + 1);

		PCGEX_FOREACH_FIELD_SPLINETOPATH(PCGEX_OUTPUT_DECL)

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade.ToSharedRef();
			PCGEX_FOREACH_FIELD_SPLINETOPATH(PCGEX_OUTPUT_INIT)
		}

		auto ApplyTransform = [&](FPCGPoint& Point, const FTransform& Transform)
		{
			if (Settings->TransformDetails.bInheritRotation && Settings->TransformDetails.bInheritScale)
			{
				Point.Transform = Transform;
			}
			else if (Settings->TransformDetails.bInheritRotation)
			{
				Point.Transform.SetLocation(Transform.GetLocation());
				Point.Transform.SetRotation(Transform.GetRotation());
			}
			else if (Settings->TransformDetails.bInheritScale)
			{
				Point.Transform.SetLocation(Transform.GetLocation());
				Point.Transform.SetScale3D(Transform.GetScale3D());
			}
			else
			{
				Point.Transform.SetLocation(Transform.GetLocation());
			}

			Point.Seed = PCGExRandom::ComputeSeed(Point);
		};

		auto GetPointType = [](EInterpCurveMode Mode)-> int32
		{
			switch (Mode)
			{
			case CIM_Linear: return 0;
			case CIM_CurveAuto: return 1;
			case CIM_Constant: return 2;
			case CIM_CurveUser: return 4;
			case CIM_CurveAutoClamped: return 3;
			default:
			case CIM_Unknown:
			case CIM_CurveBreak:
				return -1;
			}
		};

		for (int i = 0; i < NumSegments; i++)
		{
			const double LengthAtPoint = Spline.GetDistanceAlongSplineAtSplinePoint(i);
			const FTransform SplineTransform = Spline.GetTransform();

			ApplyTransform(MutablePoints[i], Spline.GetTransformAtDistanceAlongSpline(LengthAtPoint, ESplineCoordinateSpace::Type::World, true));

			int32 PtType = -1;


			PCGEX_OUTPUT_VALUE(LengthAtPoint, i, LengthAtPoint);
			PCGEX_OUTPUT_VALUE(Alpha, i, LengthAtPoint / TotalLength);
			PCGEX_OUTPUT_VALUE(ArriveTangent, i, SplineTransform.TransformVector(Spline.SplineCurves.Position.Points[i].ArriveTangent));
			PCGEX_OUTPUT_VALUE(LeaveTangent, i, SplineTransform.TransformVector(Spline.SplineCurves.Position.Points[i].LeaveTangent));
			PCGEX_OUTPUT_VALUE(PointType, i, GetPointType(Spline.SplineCurves.Position.Points[i].InterpMode));
		}

		if (Spline.bClosedLoop)
		{
			if (Settings->bTagIfClosedLoop) { PointDataFacade->Source->Tags->Add(Settings->IsClosedLoopTag); }
		}
		else
		{
			if (Settings->bTagIfOpenSpline) { PointDataFacade->Source->Tags->Add(Settings->IsOpenSplineTag); }

			ApplyTransform(MutablePoints.Last(), Spline.GetTransformAtDistanceAlongSpline(TotalLength, ESplineCoordinateSpace::Type::World, true));

			PCGEX_OUTPUT_VALUE(LengthAtPoint, LastIndex, TotalLength);
			PCGEX_OUTPUT_VALUE(Alpha, LastIndex, 1);
			PCGEX_OUTPUT_VALUE(ArriveTangent, LastIndex, Spline.SplineCurves.Position.Points[NumSegments].ArriveTangent);
			PCGEX_OUTPUT_VALUE(LeaveTangent, LastIndex, Spline.SplineCurves.Position.Points[NumSegments].LeaveTangent);
			PCGEX_OUTPUT_VALUE(PointType, LastIndex, GetPointType(Spline.SplineCurves.Position.Points[NumSegments].InterpMode));
		}

		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
