// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSpline.h"

#include "Data/PCGExPointFilter.h"

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

FPCGExSampleNearestSplineContext::~FPCGExSampleNearestSplineContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_CLEAN_SP(WeightCurve)

	Targets.Empty();
}

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
			UPCGSplineData* MutableSplineData = const_cast<UPCGSplineData*>(SplineData);

			switch (Settings->SampleInputs)
			{
			default:
			case EPCGExSplineSamplingIncludeMode::All:
				Context->Targets.Add(MutableSplineData);
				break;
			case EPCGExSplineSamplingIncludeMode::ClosedLoopOnly:
				if (MutableSplineData->SplineStruct.bClosedLoop) { Context->Targets.Add(MutableSplineData); }
				break;
			case EPCGExSplineSamplingIncludeMode::OpenSplineOnly:
				if (!MutableSplineData->SplineStruct.bClosedLoop) { Context->Targets.Add(MutableSplineData); }
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestSpline::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSampleNearestSpline::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to split."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}


namespace PCGExSampleNearestSpline
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSpline::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNearestSpline)

		PointDataFacade->bSupportsScopedGet = TypedContext->bScopedAttributeGet;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

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


		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bUseLocalRangeMin)
		{
			RangeMinGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMin);
			if (!RangeMinGetter) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMin metadata missing")); }
		}

		if (Settings->bUseLocalRangeMax)
		{
			RangeMaxGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMax);
			if (!RangeMaxGetter) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMax metadata missing")); }
		}

		if (Settings->bWriteLookAtTransform)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
			{
			} // 
			else { LookAtUpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }

			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("LookAtUp is invalid.")); }
		}

		PointIO->CreateOutKeys();

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
			const double FailSafeDist = RangeMaxGetter ? FMath::Sqrt(RangeMaxGetter->Values[Index]) : LocalSettings->RangeMax;
			PCGEX_OUTPUT_VALUE(Success, Index, false)
			PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
			PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
			PCGEX_OUTPUT_VALUE(Angle, Index, 0)
			PCGEX_OUTPUT_VALUE(Time, Index, -1)
			PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
			PCGEX_OUTPUT_VALUE(ClosedLoop, Index, false)
		};

		if (!PointFilterCache[Index])
		{
			if (LocalSettings->bProcessFilteredOutAsFails) { SamplingFailed(); }
			return;
		}


		int32 NumInside = 0;
		int32 NumInClosed = 0;
		bool bClosed = false;

		double RangeMin = FMath::Pow(RangeMinGetter ? RangeMinGetter->Values[Index] : LocalSettings->RangeMin, 2);
		double RangeMax = FMath::Pow(RangeMaxGetter ? RangeMaxGetter->Values[Index] : LocalSettings->RangeMax, 2);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExPolyLine::FSampleInfos> TargetsInfos;
		TargetsInfos.Reserve(LocalTypedContext->NumTargets);

		PCGExPolyLine::FTargetsCompoundInfos TargetsCompoundInfos;

		FVector Origin = Point.Transform.GetLocation();
		auto ProcessTarget = [&](const FTransform& Transform, const double& Time, const FPCGSplineStruct& InSpline)
		{
			const FVector ModifiedOrigin = PCGExMath::GetSpatializedCenter(LocalSettings->DistanceSettings, Point, Origin, Transform.GetLocation());
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

			if (LocalSettings->SampleMethod == EPCGExSampleMethod::ClosestTarget)
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

			if (LocalSettings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
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
			for (UPCGSplineData* Line : LocalTypedContext->Targets)
			{
				FTransform SampledTransform;
				double Time = Line->SplineStruct.FindInputKeyClosestToWorldLocation(Origin);
				SampledTransform = Line->SplineStruct.GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, false);
				ProcessTarget(SampledTransform, Time, Line->SplineStruct);
			}
		}
		else
		{
			for (UPCGSplineData* Line : LocalTypedContext->Targets)
			{
				FTransform SampledTransform;
				double Time = Line->SplineStruct.FindInputKeyClosestToWorldLocation(Origin);
				SampledTransform = Line->SplineStruct.GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, false);
				ProcessTarget(SampledTransform, Time, Line->SplineStruct);
			}
		}

		// Compound never got updated, meaning we couldn't find target in range
		if (TargetsCompoundInfos.UpdateCount <= 0)
		{
			SamplingFailed();
			return;
		}

		// Compute individual target weight
		if (LocalSettings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0)
		{
			// Reset compounded infos to full range
			TargetsCompoundInfos.SampledRangeMin = RangeMin;
			TargetsCompoundInfos.SampledRangeMax = RangeMax;
			TargetsCompoundInfos.SampledRangeWidth = RangeMax - RangeMin;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);

		FVector WeightedUp = SafeUpVector;
		if (LocalSettings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Values[Index]; }

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

			if (LocalSettings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += PCGExMath::GetDirection(Quat, LocalSettings->LookAtUpAxis) * Weight; }

			WeightedSignAxis += PCGExMath::GetDirection(Quat, LocalSettings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(Quat, LocalSettings->AngleAxis) * Weight;
			WeightedTime += TargetInfos.Time * Weight;
			TotalWeight += Weight;
		};


		if (LocalSettings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			LocalSettings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			const PCGExPolyLine::FSampleInfos& TargetInfos = LocalSettings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
			const double Weight = LocalTypedContext->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExPolyLine::FSampleInfos& TargetInfos : TargetsInfos)
			{
				const double Weight = LocalTypedContext->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
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
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, LocalSettings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, (!bOnlySignIfClosed || NumInClosed > 0) ? FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance : WeightedDistance)
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(LocalSettings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(Time, Index, WeightedTime)
		PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
		PCGEX_OUTPUT_VALUE(ClosedLoop, Index, bClosed)

		FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);

		if (LocalSettings->bTagIfHasSuccesses && bAnySuccess) { PointIO->Tags->Add(LocalSettings->HasSuccessesTag); }
		if (LocalSettings->bTagIfHasNoSuccesses && !bAnySuccess) { PointIO->Tags->Add(LocalSettings->HasNoSuccessesTag); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
