// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSpline.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestSplineElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

namespace PCGExPolyLine
{
	void FSamplesStats::Update(const FSample& Infos, bool& IsNewClosest, bool& IsNewFarthest)
	{
		UpdateCount++;

		if (Infos.Distance < SampledRangeMin)
		{
			Closest = Infos;
			SampledRangeMin = Infos.Distance;
			IsNewClosest = true;
		}

		if (Infos.Distance > SampledRangeMax)
		{
			Farthest = Infos;
			SampledRangeMax = Infos.Distance;
			IsNewFarthest = true;
		}

		SampledRangeWidth = SampledRangeMax - SampledRangeMin;
	}
}

UPCGExSampleNearestSplineSettings::UPCGExSampleNearestSplineSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestSplineSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(PCGEx::SourceTargetsLabel, "The spline data set to check against.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleNearestSplineSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

int32 UPCGExSampleNearestSplineSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestSpline)

bool FPCGExSampleNearestSplineElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSpline)

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);

	Context->DistanceDetails = PCGExDetails::MakeDistances(Settings->DistanceSettings, Settings->DistanceSettings);

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
	for (const UPCGSplineData* SplineData : Context->Targets) { Context->Splines.Add(&SplineData->SplineStruct); }

	Context->SegmentCounts.SetNumUninitialized(Context->NumTargets);
	for (int i = 0; i < Context->NumTargets; i++) { Context->SegmentCounts[i] = Context->Targets[i]->SplineStruct.GetNumberOfSplineSegments(); }

	Context->RuntimeWeightCurve = Settings->LocalWeightOverDistance;
	if (!Settings->bUseLocalCurve)
	{
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(0, 0);
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(1, 1);
		Context->RuntimeWeightCurve.ExternalCurve = Settings->WeightOverDistance.LoadSynchronous();
	}
	Context->WeightCurve = Context->RuntimeWeightCurve.GetRichCurveConst();

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSampleNearestSplineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSplineElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSpline)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestSpline::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleNearestSpline::FProcessor>>& NewBatch)
			{
				if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to split."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}


namespace PCGExSampleNearestSpline
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSpline::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());

		if (Settings->SampleInputs != EPCGExSplineSamplingIncludeMode::All)
		{
			bOnlySignIfClosed = Settings->bOnlySignIfClosed;
			bOnlyIncrementInsideNumIfClosed = Settings->bOnlyIncrementInsideNumIfClosed;
		}
		else
		{
			bOnlySignIfClosed = false;
			bOnlyIncrementInsideNumIfClosed = false;
		}

		SafeUpVector = Settings->LookAtUpConstant;

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bUseLocalRangeMin)
		{
			RangeMinGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMin);
			if (!RangeMinGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("RangeMin metadata missing")); }
		}

		if (Settings->bUseLocalRangeMax)
		{
			RangeMaxGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMax);
			if (!RangeMaxGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("RangeMax metadata missing")); }
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection == EPCGExSampleSource::Source)
		{
			LookAtUpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource);
			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("LookAtUp is invalid.")); }
		}

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bClosestSample = Settings->SampleMethod != EPCGExSampleMethod::FarthestTarget;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::SamplingFailed(const int32 Index, const FPCGPoint& Point, const double InDepth)
	{
		SampleState[Index] = false;

		const double FailSafeDist = RangeMaxGetter ? FMath::Sqrt(RangeMaxGetter->Read(Index)) : Settings->RangeMax;
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(Depth, Index, Settings->bInvertDepth ? 1-InDepth : InDepth)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(Angle, Index, 0)
		PCGEX_OUTPUT_VALUE(Time, Index, -1)
		PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
		PCGEX_OUTPUT_VALUE(ClosedLoop, Index, false)
	}

	void FProcessor::ProcessSinglePoint(int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index, Point, 0); }
			return;
		}

		int32 NumInside = 0;
		int32 NumSampled = 0;
		int32 NumInClosed = 0;

		bool bClosed = false;

		double BaseRangeMin = RangeMinGetter ? RangeMinGetter->Read(Index) : Settings->RangeMin;
		double BaseRangeMax = RangeMaxGetter ? RangeMaxGetter->Read(Index) : Settings->RangeMax;

		if (BaseRangeMin > BaseRangeMax) { std::swap(BaseRangeMin, BaseRangeMax); }

		double RangeMin = BaseRangeMin;
		double RangeMax = BaseRangeMax;
		double Depth = MAX_dbl;
		double DepthSamples = Settings->DepthMode == EPCGExSplineDepthMode::Average ? 0 : 1;

		if (Settings->DepthMode == EPCGExSplineDepthMode::Max || Settings->DepthMode == EPCGExSplineDepthMode::Average) { Depth = 0; }

		TArray<PCGExPolyLine::FSample> Samples;
		Samples.Reserve(Context->NumTargets);

		PCGExPolyLine::FSamplesStats Stats;

		FVector Origin = Point.Transform.GetLocation();
		auto ProcessTarget = [&](const FTransform& Transform, const double& Time, const FPCGSplineStruct* InSpline)
		{
			const FVector SampleLocation = Transform.GetLocation();
			const FVector ModifiedOrigin = Context->DistanceDetails->GetSourceCenter(Point, Origin, SampleLocation);
			const double Dist = FVector::Dist(ModifiedOrigin, SampleLocation);

			double RMin = BaseRangeMin;
			double RMax = BaseRangeMax;
			double DepthRange = Settings->DepthRange;

			if (Settings->bSplineScalesRanges)
			{
				const FVector S = Transform.GetScale3D();
				const double RScale = FVector2D(S.Y, S.Z).Length();
				RMin *= RScale;
				RMax *= RScale;
				DepthRange *= RScale;
			}

			if (Settings->bWriteDepth)
			{
				switch (Settings->DepthMode)
				{
				default:
				case EPCGExSplineDepthMode::Min:
					Depth = FMath::Min(Depth, FMath::Clamp(Dist, 0, DepthRange) / DepthRange);
					break;
				case EPCGExSplineDepthMode::Max:
					Depth = FMath::Max(Depth, FMath::Clamp(Dist, 0, DepthRange) / DepthRange);
					break;
				case EPCGExSplineDepthMode::Average:
					Depth += FMath::Clamp(Dist, 0, DepthRange);
					DepthSamples++;
					break;
				}
			}

			if (RMax > 0 && (Dist < RMin || Dist > RMax)) { return; }

			int32 NumInsideIncrement = 0;

			if (FVector::DotProduct((SampleLocation - ModifiedOrigin).GetSafeNormal(), Transform.GetRotation().GetRightVector()) > 0)
			{
				if (!bOnlyIncrementInsideNumIfClosed || InSpline->bClosedLoop) { NumInsideIncrement = 1; }
			}

			bool IsNewClosest = false;
			bool IsNewFarthest = false;

			if (bSingleSample)
			{
				Stats.Update(PCGExPolyLine::FSample(Transform, Dist, Time), IsNewClosest, IsNewFarthest);

				if ((bClosestSample && !IsNewClosest) || !IsNewFarthest) { return; }

				bClosed = InSpline->bClosedLoop;

				NumInside = NumInsideIncrement;
				NumInClosed = NumInsideIncrement;

				RangeMin = RMin;
				RangeMax = RMax;
			}
			else
			{
				const PCGExPolyLine::FSample& Infos = Samples.Emplace_GetRef(Transform, Dist, Time);
				Stats.Update(Infos, IsNewClosest, IsNewFarthest);

				if (InSpline->bClosedLoop)
				{
					bClosed = true;
					NumInClosed++;
				}

				NumInside += NumInsideIncrement;

				RangeMin = FMath::Min(RangeMin, RMin);
				RangeMax = FMath::Max(RangeMax, RMax);
			}
		};

		// First: Sample all possible targets
		for (int i = 0; i < Context->NumTargets; i++)
		{
			const FPCGSplineStruct* Line = Context->Splines[i];
			FTransform SampledTransform;
			double Time = Line->FindInputKeyClosestToWorldLocation(Origin);
			SampledTransform = Line->GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, Settings->bSplineScalesRanges);
			ProcessTarget(SampledTransform, Time / Context->SegmentCounts[i], Line);
		}

		Depth /= DepthSamples;

		// Compound never got updated, meaning we couldn't find target in range
		if (Stats.UpdateCount <= 0)
		{
			SamplingFailed(Index, Point, Depth);
			return;
		}

		// Compute individual target weight
		if (Settings->WeightMethod == EPCGExRangeType::FullRange && BaseRangeMax > 0)
		{
			// Reset compounded infos to full range
			Stats.SampledRangeMin = RangeMin;
			Stats.SampledRangeMax = RangeMax;
			Stats.SampledRangeWidth = RangeMax - RangeMin;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);

		FVector WeightedUp = SafeUpVector;
		if (LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double WeightedTime = 0;
		double TotalWeight = 0;

		auto ProcessTargetInfos = [&](const PCGExPolyLine::FSample& Sample, const double Weight)
		{
			const FQuat Quat = Sample.Transform.GetRotation();

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() + (Sample.Transform.GetRotation() * Weight));
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() + (Sample.Transform.GetScale3D() * Weight));
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() + (Sample.Transform.GetLocation() * Weight));

			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += PCGExMath::GetDirection(Quat, Settings->LookAtUpAxis) * Weight; }

			WeightedSignAxis += PCGExMath::GetDirection(Quat, Settings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(Quat, Settings->AngleAxis) * Weight;
			WeightedTime += Sample.Time * Weight;
			TotalWeight += Weight;

			NumSampled++;
		};


		if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			const PCGExPolyLine::FSample& TargetInfos = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? Stats.Closest : Stats.Farthest;
			const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExPolyLine::FSample& TargetInfos : Samples)
			{
				const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
				if (Weight == 0) { continue; }
				ProcessTargetInfos(TargetInfos, Weight);
			}
		}

		if (TotalWeight != 0) // Dodge NaN
		{
			WeightedUp /= TotalWeight;

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() / TotalWeight);
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() / TotalWeight);
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() / TotalWeight);
		}
		else
		{
			WeightedUp = WeightedUp.GetSafeNormal();
			WeightedTransform = Point.Transform;
		}

		WeightedUp.Normalize();

		FVector LookAt = (Point.Transform.GetLocation() - WeightedTransform.GetLocation()).GetSafeNormal();
		const double WeightedDistance = FVector::Dist(Origin, WeightedTransform.GetLocation());

		SampleState[Index] = Stats.IsValid();
		PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(Depth, Index, Settings->bInvertDepth ? 1 - Depth : Depth)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, (!bOnlySignIfClosed || NumInClosed > 0) ? FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance : WeightedDistance)
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(Time, Index, WeightedTime)
		PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, NumSampled)
		PCGEX_OUTPUT_VALUE(ClosedLoop, Index, bClosed)

		FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->Add(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->Add(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		PCGExSampling::PruneFailedSamples(PointDataFacade->GetMutablePoints(), SampleState);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
