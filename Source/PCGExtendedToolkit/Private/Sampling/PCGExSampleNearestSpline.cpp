﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSpline.h"

#include "PCGExScopedContainers.h"
#include "Data/PCGExDataTag.h"
#include "Data/Blending//PCGExBlendModes.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSplineElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

PCGEX_SETTING_VALUE_IMPL(UPCGExSampleNearestSplineSettings, RangeMin, double, RangeMinInput, RangeMinAttribute, RangeMin)
PCGEX_SETTING_VALUE_IMPL(UPCGExSampleNearestSplineSettings, RangeMax, double, RangeMaxInput, RangeMaxAttribute, RangeMax)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExSampleNearestSplineSettings, SampleAlpha, double, bSampleSpecificAlpha, SampleAlphaAttribute, SampleAlphaConstant)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExSampleNearestSplineSettings, LookAtUp, FVector, LookAtUpSelection != EPCGExSampleSource::Constant, LookAtUpSource, LookAtUpConstant)

namespace PCGExPolyPath
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
	PCGEX_PIN_POLYLINES(PCGEx::SourceTargetsLabel, "The spline data set to check against.", Required)
	return PinProperties;
}

void FPCGExSampleNearestSplineContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleNearestSpline)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightOverDistance.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestSpline)
PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleNearestSpline)

bool FPCGExSampleNearestSplineElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSpline)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);

	Context->DistanceDetails = PCGExDetails::MakeDistances(Settings->DistanceSettings, Settings->DistanceSettings);

	if (!Targets.IsEmpty())
	{
		for (const FPCGTaggedData& TaggedData : Targets)
		{
			const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
			if (!SplineData || SplineData->SplineStruct.GetNumberOfSplineSegments() <= 0) { continue; }

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
		PCGE_LOG(Error, GraphAndLog, FTEXT("No targets (no input matches criteria or empty dataset)"));
		return false;
	}

	Context->Splines.Reserve(Context->NumTargets);
	for (const UPCGSplineData* SplineData : Context->Targets) { Context->Splines.Add(SplineData->SplineStruct); }

	TArray<FBox> SplineBounds;

	SplineBounds.Reserve(Context->NumTargets);
	Context->SegmentCounts.SetNumUninitialized(Context->NumTargets);
	Context->Lengths.SetNumUninitialized(Context->NumTargets);

	TArray<FVector> SplinePoints;
	for (int i = 0; i < Context->NumTargets; i++)
	{
		const FPCGSplineStruct& Spline = Context->Targets[i]->SplineStruct;
		Context->SegmentCounts[i] = Spline.GetNumberOfSplineSegments();
		Context->Lengths[i] = Spline.GetSplineLength();

		if (Settings->bUseOctree)
		{
			Spline.ConvertSplineToPolyLine(ESplineCoordinateSpace::World, FMath::Square(50), SplinePoints);

			FBox& Box = SplineBounds.Emplace_GetRef(ForceInit);
			for (const FVector& Point : SplinePoints) { Box += Point; }
			Context->OctreeBounds += Box;
		}
	}

	if (Settings->bUseOctree)
	{
		Context->SplineOctree = MakeShared<PCGExOctree::FItemOctree>(Context->OctreeBounds.GetCenter(), Context->OctreeBounds.GetExtent().Length());
		for (int i = 0; i < Context->NumTargets; i++) { Context->SplineOctree->AddElement(PCGExOctree::FItem(i, SplineBounds[i])); }
	}

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->bComputeTangents = Settings->bWriteArriveTangent || Settings->bWriteLeaveTangent;

	return true;
}

void FPCGExSampleNearestSplineElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSpline)

	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	Context->RuntimeWeightCurve = Settings->LocalWeightOverDistance;

	if (!Settings->bUseLocalCurve)
	{
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(0, 0);
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(1, 1);
		Context->RuntimeWeightCurve.ExternalCurve = Settings->WeightOverDistance.Get();
	}

	Context->WeightCurve = Context->RuntimeWeightCurve.GetRichCurveConst();
}

bool FPCGExSampleNearestSplineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSplineElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSpline)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to split."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExSampleNearestSplineElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}


namespace PCGExSampleNearestSpline
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSpline::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;

		if (Context->ApplySampling.WantsApply())
		{
			AllocateFor |= EPCGPointNativeProperties::Transform;
		}

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);


		DistanceDetails = Context->DistanceDetails;
		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

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

		RangeMinGetter = Settings->GetValueSettingRangeMin();
		if (!RangeMinGetter->Init(PointDataFacade)) { return false; }

		RangeMaxGetter = Settings->GetValueSettingRangeMax();
		if (!RangeMaxGetter->Init(PointDataFacade)) { return false; }

		if (Settings->bSampleSpecificAlpha)
		{
			SampleAlphaGetter = Settings->GetValueSettingSampleAlpha();
			if (!SampleAlphaGetter->Init(PointDataFacade)) { return false; }
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection == EPCGExSampleSource::Source)
		{
			LookAtUpGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->LookAtUpSource, true);
			if (!LookAtUpGetter) { PCGEX_LOG_INVALID_SELECTOR_C(Context, LookAt Up, Settings->LookAtUpSource) }
		}

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bClosestSample = Settings->SampleMethod != EPCGExSampleMethod::FarthestTarget;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExSampleNearestSplineContext, UPCGExSampleNearestSplineSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::SamplingFailed(const int32 Index, const double InDepth)
	{
		SamplingMask[Index] = false;

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		const double FailSafeDist = RangeMaxGetter->Read(Index);
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, InTransforms[Index])
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, InTransforms[Index])
		PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? FailSafeDist : FailSafeDist * Settings->DistanceScale)
		PCGEX_OUTPUT_VALUE(Depth, Index, Settings->bInvertDepth ? 1-InDepth : InDepth)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist * Settings->SignedDistanceScale)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, FVector(FailSafeDist))
		PCGEX_OUTPUT_VALUE(Angle, Index, 0)
		PCGEX_OUTPUT_VALUE(Time, Index, -1)
		PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
		PCGEX_OUTPUT_VALUE(ClosedLoop, Index, false)
		PCGEX_OUTPUT_VALUE(ArriveTangent, Index, FVector::ZeroVector)
		PCGEX_OUTPUT_VALUE(LeaveTangent, Index, FVector::ZeroVector)
		PCGEX_OUTPUT_VALUE(TotalWeight, Index, -1)
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestSpline::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bAnySuccessLocal = false;

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		TArray<PCGExPolyPath::FSample> Samples;
		Samples.Reserve(Context->NumTargets);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index, 0); }
				continue;
			}

			int32 NumInside = 0;
			int32 NumSampled = 0;
			int32 NumInClosed = 0;

			bool bSampledClosedLoop = false;

			double BaseRangeMin = RangeMinGetter->Read(Index);
			double BaseRangeMax = RangeMaxGetter->Read(Index);
			if (BaseRangeMin > BaseRangeMax) { std::swap(BaseRangeMin, BaseRangeMax); }

			double MinSampledRange = BaseRangeMin;
			double MaxSampledRange = BaseRangeMax;
			double Depth = MAX_dbl;
			double DepthSamples = Settings->DepthMode == EPCGExSplineDepthMode::Average ? 0 : 1;
			double WeightedDistance = 0;

			if (Settings->DepthMode == EPCGExSplineDepthMode::Max || Settings->DepthMode == EPCGExSplineDepthMode::Average) { Depth = 0; }

			Samples.Reset();

			PCGExPolyPath::FSamplesStats Stats;

			FVector Origin = InTransforms[Index].GetLocation();
			PCGExData::FConstPoint Point = PointDataFacade->GetInPoint(Index);

			auto ProcessTarget = [&](const FTransform& Transform, const double& Time, const int32 NumSegments, const FPCGSplineStruct& InSpline)
			{
				const FVector SampleLocation = Transform.GetLocation();
				const FVector ModifiedOrigin = DistanceDetails->GetSourceCenter(Point, Origin, SampleLocation);
				const double Dist = FVector::Dist(ModifiedOrigin, SampleLocation);

				double LocalRangeMin = BaseRangeMin;
				double LocalRangeMax = BaseRangeMax;
				double DepthRange = Settings->DepthRange;

				if (Settings->bSplineScalesRanges)
				{
					const FVector S = Transform.GetScale3D();
					const double RScale = FVector2D(S.Y, S.Z).Length();
					LocalRangeMin *= RScale;
					LocalRangeMax *= RScale;
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

				if (LocalRangeMax > 0 && (Dist < LocalRangeMin || Dist > LocalRangeMax)) { return; }

				int32 NumInsideIncrement = 0;

				if (FVector::DotProduct((SampleLocation - ModifiedOrigin).GetSafeNormal(), Transform.GetRotation().GetRightVector()) > 0)
				{
					if (!bOnlyIncrementInsideNumIfClosed || InSpline.bClosedLoop) { NumInsideIncrement = 1; }
				}

				bool IsNewClosest = false;
				bool IsNewFarthest = false;

				const double NormalizedTime = Time / static_cast<double>(NumSegments);
				PCGExPolyPath::FSample Infos(Transform, Dist, NormalizedTime);

				if (Context->bComputeTangents)
				{
					const int32 PrevIndex = FMath::FloorToInt(Time);
					const int32 NextIndex = InSpline.bClosedLoop ? PCGExMath::Tile(PrevIndex + 1, 0, NumSegments - 1) : FMath::Clamp(PrevIndex + 1, 0, NumSegments);

					const FInterpCurveVector& SplinePositions = InSpline.GetSplinePointsPosition();
					Infos.Tangent = Transform.GetRotation().GetForwardVector() * FMath::Lerp(
						SplinePositions.Points[PrevIndex].ArriveTangent.Length(),
						SplinePositions.Points[NextIndex].LeaveTangent.Length(), Time - PrevIndex);
				}

				if (bSingleSample)
				{
					Stats.Update(Infos, IsNewClosest, IsNewFarthest);

					if ((bClosestSample && !IsNewClosest) || !IsNewFarthest) { return; }

					bSampledClosedLoop = InSpline.bClosedLoop;

					NumInside = NumInsideIncrement;
					NumInClosed = NumInsideIncrement;

					MinSampledRange = LocalRangeMin;
					MaxSampledRange = LocalRangeMax;
				}
				else
				{
					Samples.Add(Infos);
					Stats.Update(Infos, IsNewClosest, IsNewFarthest);

					if (InSpline.bClosedLoop)
					{
						bSampledClosedLoop = true;
						NumInClosed += NumInsideIncrement;
					}

					NumInside += NumInsideIncrement;

					MinSampledRange = FMath::Min(MinSampledRange, LocalRangeMin);
					MaxSampledRange = FMath::Max(MaxSampledRange, LocalRangeMax);
				}
			};

			// First: Sample all valid targets
			if (!Settings->bSampleSpecificAlpha)
			{
				auto ProcessClosestAlpha = [&](const int32 TargetIndex)
				{
					const FPCGSplineStruct& Line = Context->Splines[TargetIndex];
					const double Time = Line.FindInputKeyClosestToWorldLocation(Origin);
					ProcessTarget(
						Line.GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, Settings->bSplineScalesRanges),
						Time, Context->SegmentCounts[TargetIndex], Line);
				};

				// At closest alpha
				if (Settings->bUseOctree)
				{
					Context->SplineOctree->FindElementsWithBoundsTest(
						FBox(Origin - FVector(BaseRangeMax), Origin + FVector(BaseRangeMax)),
						[&](const PCGExOctree::FItem& Item) { ProcessClosestAlpha(Item.Index); });
				}
				else
				{
					for (int i = 0; i < Context->NumTargets; i++) { ProcessClosestAlpha(i); }
				}
			}
			else
			{
				const double InputKey = SampleAlphaGetter->Read(Index);
				auto ProcessSpecificAlpha = [&](const int32 TargetIndex)
				{
					const FPCGSplineStruct& Line = Context->Splines[TargetIndex];
					const double NumSegments = Context->SegmentCounts[TargetIndex];
					double Time = 0;

					switch (Settings->SampleAlphaMode)
					{
					default:
					case EPCGExSplineSampleAlphaMode::Alpha:
						Time = InputKey * NumSegments;
						break;
					case EPCGExSplineSampleAlphaMode::Time:
						Time = InputKey / NumSegments;
						break;
					case EPCGExSplineSampleAlphaMode::Distance:
						Time = (InputKey / Context->Lengths[TargetIndex]) * NumSegments;
						break;
					}

					if (Settings->bWrapClosedLoopAlpha && Line.bClosedLoop) { Time = PCGExMath::Tile(Time, 0.0, NumSegments); }
					ProcessTarget(Line.GetTransformAtSplineInputKey(static_cast<float>(Time), ESplineCoordinateSpace::World, Settings->bSplineScalesRanges), Time, NumSegments, Line);
				};

				// At specific alpha
				if (Settings->bUseOctree)
				{
					Context->SplineOctree->FindElementsWithBoundsTest(
						FBox(Origin - FVector(BaseRangeMax), Origin + FVector(BaseRangeMax)),
						[&](const PCGExOctree::FItem& Item) { ProcessSpecificAlpha(Item.Index); });
				}
				else
				{
					for (int i = 0; i < Context->NumTargets; i++) { ProcessSpecificAlpha(i); }
				}
			}

			Depth /= DepthSamples;

			// Compound never got updated, meaning we couldn't find target in range
			if (Stats.UpdateCount <= 0)
			{
				SamplingFailed(Index, Depth);
				continue;
			}

			// Compute individual target weight
			if (Settings->WeightMethod == EPCGExRangeType::FullRange && BaseRangeMax > 0)
			{
				// Reset compounded infos to full range
				Stats.SampledRangeMin = MinSampledRange;
				Stats.SampledRangeMax = MaxSampledRange;
				Stats.SampledRangeWidth = MaxSampledRange - MinSampledRange;
			}

			FVector WeightedUp = SafeUpVector;
			if (LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

			FTransform WeightedTransform = InTransforms[Index];
			FVector WeightedSignAxis = FVector::ZeroVector;
			FVector WeightedAngleAxis = FVector::ZeroVector;
			FVector WeightedTangent = FVector::ZeroVector;

			double WeightedTime = 0;
			double TotalWeight = 0;


			if (!Settings->bWeightFromOriginalTransform)
			{
				WeightedTransform = FTransform::Identity;
				WeightedTransform.SetScale3D(FVector::ZeroVector);
			}

			auto ProcessTargetInfos = [&](const PCGExPolyPath::FSample& TargetInfos, const double Weight)
			{
				const FQuat Quat = TargetInfos.Transform.GetRotation();

				WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, TargetInfos.Transform, Weight);
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { PCGExBlend::WeightedAdd(WeightedUp, PCGExMath::GetDirection(Quat, Settings->LookAtUpAxis), Weight); }

				WeightedSignAxis += PCGExMath::GetDirection(Quat, Settings->SignAxis) * Weight;
				WeightedAngleAxis += PCGExMath::GetDirection(Quat, Settings->AngleAxis) * Weight;
				WeightedTangent = PCGExBlend::WeightedAdd(WeightedTangent, TargetInfos.Tangent, Weight);
				WeightedTime += TargetInfos.Time * Weight;
				TotalWeight += Weight;
				WeightedDistance += TargetInfos.Distance;

				NumSampled++;
			};


			if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
				Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				const PCGExPolyPath::FSample& TargetInfos = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? Stats.Closest : Stats.Farthest;
				const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
				ProcessTargetInfos(TargetInfos, Weight);
			}
			else
			{
				for (PCGExPolyPath::FSample& TargetInfos : Samples)
				{
					const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
					if (Weight == 0) { continue; }
					ProcessTargetInfos(TargetInfos, Weight);
				}
			}

			if (TotalWeight != 0) // Dodge NaN
			{
				//WeightedUp /= TotalWeight;
				//WeightedTransform = PCGExBlend::Div(WeightedTransform, TotalWeight);
			}
			else
			{
				WeightedUp = WeightedUp.GetSafeNormal();
				WeightedTransform = InTransforms[Index];
			}

			WeightedDistance /= NumSampled;
			WeightedUp.Normalize();

			const FVector CWDistance = Origin - WeightedTransform.GetLocation();
			FVector LookAt = CWDistance.GetSafeNormal();

			FTransform LookAtTransform = PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign);
			if (Context->ApplySampling.WantsApply())
			{
				PCGExData::FMutablePoint MutablePoint = PointDataFacade->GetOutPoint(Index);
				Context->ApplySampling.Apply(MutablePoint, WeightedTransform, LookAtTransform);
			}

			SamplingMask[Index] = Stats.IsValid();
			PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
			PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, LookAtTransform)
			PCGEX_OUTPUT_VALUE(ArriveTangent, Index, WeightedTangent)
			PCGEX_OUTPUT_VALUE(LeaveTangent, Index, WeightedTangent)
			PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? WeightedDistance : WeightedDistance * Settings->DistanceScale)
			PCGEX_OUTPUT_VALUE(Depth, Index, Settings->bInvertDepth ? 1 - Depth : Depth)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, (!bOnlySignIfClosed || NumInClosed > 0) ? FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance : WeightedDistance * Settings->SignedDistanceScale)
			PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
			PCGEX_OUTPUT_VALUE(Time, Index, WeightedTime)
			PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
			PCGEX_OUTPUT_VALUE(NumSamples, Index, NumSampled)
			PCGEX_OUTPUT_VALUE(ClosedLoop, Index, bSampledClosedLoop)
			PCGEX_OUTPUT_VALUE(TotalWeight, Index, TotalWeight)

			MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), WeightedDistance));
			bAnySuccessLocal = true;
		}

		if (bAnySuccessLocal) { FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!Settings->bOutputNormalizedDistance || !DistanceWriter) { return; }

		MaxDistance = MaxDistanceValue->Max();

		const int32 NumPoints = PointDataFacade->GetNum();

		if (Settings->bOutputOneMinusDistance)
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const double D = DistanceWriter->GetValue(i);
				DistanceWriter->SetValue(i, (1 - (D / MaxDistance)) * Settings->DistanceScale);
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const double D = DistanceWriter->GetValue(i);
				DistanceWriter->SetValue(i, (D / MaxDistance) * Settings->DistanceScale);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		if (Settings->bPruneFailedSamples) { (void)PointDataFacade->Source->Gather(SamplingMask); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
