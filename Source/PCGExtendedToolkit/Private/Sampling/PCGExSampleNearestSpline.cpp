// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSpline.h"

/////*BUILD_TOOL_BUG_55_TOGGLE*/#include "CoreMinimal.h"
#define LOCTEXT_NAMESPACE "PCGExSampleNearestSplineElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

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

PCGExData::EInit UPCGExSampleNearestSplineSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestSplineSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestSpline)

bool FPCGExSampleNearestSplineElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSpline)

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

	Context->SegmentCounts.SetNumUninitialized(Context->NumTargets);
	for (int i = 0; i < Context->NumTargets; i++) { Context->SegmentCounts[i] = Context->Targets[i]->SplineStruct.GetNumberOfSplineSegments(); }

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();
	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

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

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		auto SamplingFailed = [&]()
		{
			const double FailSafeDist = RangeMaxGetter ? FMath::Sqrt(RangeMaxGetter->Read(Index)) : Settings->RangeMax;
			PCGEX_OUTPUT_VALUE(Success, Index, false)
			PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
			PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
			PCGEX_OUTPUT_VALUE(Angle, Index, 0)
			PCGEX_OUTPUT_VALUE(Time, Index, -1)
			PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
			PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
			PCGEX_OUTPUT_VALUE(ClosedLoop, Index, false)
		};

		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(); }
			return;
		}


		int32 NumInside = 0;
		int32 NumSampled = 0;
		int32 NumInClosed = 0;
		bool bClosed = false;

		double RangeMin = FMath::Pow(RangeMinGetter ? RangeMinGetter->Read(Index) : Settings->RangeMin, 2);
		double RangeMax = FMath::Pow(RangeMaxGetter ? RangeMaxGetter->Read(Index) : Settings->RangeMax, 2);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExPolyLine::FSampleInfos> TargetsInfos;
		TargetsInfos.Reserve(Context->NumTargets);

		PCGExPolyLine::FTargetsCompoundInfos TargetsCompoundInfos;

		FVector Origin = Point.Transform.GetLocation();
		auto ProcessTarget = [&](const FTransform& Transform, const double& Time, const FPCGSplineStruct& InSpline)
		{
			const FVector ModifiedOrigin = PCGExMath::GetSpatializedCenter(Settings->DistanceSettings, Point, Origin, Transform.GetLocation());
			const double Dist = FVector::DistSquared(ModifiedOrigin, Transform.GetLocation());

			if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

			int32 NumInsideIncrement = 0;

			if (FVector::DotProduct(
				(Transform.GetLocation() - ModifiedOrigin).GetSafeNormal(),
				Transform.GetRotation().GetRightVector()) > 0)
			{
				if (!bOnlyIncrementInsideNumIfClosed || InSpline.bClosedLoop) { NumInsideIncrement = 1; }
			}

			bool IsNewClosest = false;
			bool IsNewFarthest = false;

			if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
			{
				TargetsCompoundInfos.UpdateCompound(PCGExPolyLine::FSampleInfos(Transform, Dist, Time), IsNewClosest, IsNewFarthest);
				if (IsNewClosest)
				{
					bClosed = InSpline.bClosedLoop;
					NumInside = NumInsideIncrement;
					NumInClosed = NumInsideIncrement;
				}
				return;
			}

			if (Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				TargetsCompoundInfos.UpdateCompound(PCGExPolyLine::FSampleInfos(Transform, Dist, Time), IsNewClosest, IsNewFarthest);
				if (IsNewFarthest)
				{
					bClosed = InSpline.bClosedLoop;
					NumInside = NumInsideIncrement;
					NumInClosed = NumInsideIncrement;
				}
				return;
			}

			NumInside += NumInsideIncrement;
			if (InSpline.bClosedLoop)
			{
				NumInClosed++;
				bClosed = true;
			}

			const PCGExPolyLine::FSampleInfos& Infos = TargetsInfos.Emplace_GetRef(Transform, Dist, Time);
			TargetsCompoundInfos.UpdateCompound(Infos, IsNewClosest, IsNewFarthest);
		};

		// First: Sample all possible targets
		if (RangeMax > 0)
		{
			for (int i = 0; i < Context->NumTargets; i++)
			{
				const FPCGSplineStruct& Line = Context->Splines[i];
				FTransform SampledTransform;
				double Time = Line.FindInputKeyClosestToWorldLocation(Origin);
				SampledTransform = Line.GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, false);
				ProcessTarget(SampledTransform, Time / Context->SegmentCounts[i], Line);
			}
		}
		else
		{
			for (int i = 0; i < Context->NumTargets; i++)
			{
				const FPCGSplineStruct& Line = Context->Splines[i];
				FTransform SampledTransform;
				double Time = Line.FindInputKeyClosestToWorldLocation(Origin);
				SampledTransform = Line.GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, false);
				ProcessTarget(SampledTransform, Time / Context->SegmentCounts[i], Line);
			}
		}

		// Compound never got updated, meaning we couldn't find target in range
		if (TargetsCompoundInfos.UpdateCount <= 0)
		{
			SamplingFailed();
			return;
		}

		// Compute individual target weight
		if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0)
		{
			// Reset compounded infos to full range
			TargetsCompoundInfos.SampledRangeMin = RangeMin;
			TargetsCompoundInfos.SampledRangeMax = RangeMax;
			TargetsCompoundInfos.SampledRangeWidth = RangeMax - RangeMin;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);

		FVector WeightedUp = SafeUpVector;
		if (LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double WeightedTime = 0;
		double TotalWeight = 0;

		auto ProcessTargetInfos = [&](const PCGExPolyLine::FSampleInfos& TargetInfos, const double Weight)
		{
			const FQuat Quat = TargetInfos.Transform.GetRotation();

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() + (TargetInfos.Transform.GetRotation() * Weight));
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() + (TargetInfos.Transform.GetScale3D() * Weight));
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() + (TargetInfos.Transform.GetLocation() * Weight));

			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += PCGExMath::GetDirection(Quat, Settings->LookAtUpAxis) * Weight; }

			WeightedSignAxis += PCGExMath::GetDirection(Quat, Settings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(Quat, Settings->AngleAxis) * Weight;
			WeightedTime += TargetInfos.Time * Weight;
			TotalWeight += Weight;

			NumSampled++;
		};


		if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			const PCGExPolyLine::FSampleInfos& TargetInfos = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
			const double Weight = Context->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExPolyLine::FSampleInfos& TargetInfos : TargetsInfos)
			{
				const double Weight = Context->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
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

		PCGEX_OUTPUT_VALUE(Success, Index, TargetsCompoundInfos.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
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
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
