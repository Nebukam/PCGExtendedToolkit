﻿// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPolyline.h"

#include "Data/PCGExDataFilter.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPolylineElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

UPCGExSampleNearestPolylineSettings::UPCGExSampleNearestPolylineSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPolylineSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POLYLINES(PCGEx::SourceTargetsLabel, "The spline data set to check against.", Required, {})
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestPolylineSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestPolylineSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestPolyline)

FPCGExSampleNearestPolylineContext::~FPCGExSampleNearestPolylineContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PointFilterManager)
	PointFilterFactories.Empty();

	PCGEX_DELETE(RangeMinGetter)
	PCGEX_DELETE(RangeMaxGetter)
	PCGEX_DELETE(LookAtUpGetter)

	PCGEX_DELETE(Targets)

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_DELETE)
}

bool FPCGExSampleNearestPolylineElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPolyline)

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);

	if (!Targets.IsEmpty()) { Context->Targets = new PCGExData::FPolyLineIOGroup(Targets); }

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	Context->RangeMinGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->RangeMinGetter->Capture(Settings->LocalRangeMin);

	Context->RangeMaxGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->RangeMaxGetter->Capture(Settings->LocalRangeMax);

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_FWD)

	if (!Context->Targets || Context->Targets->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No targets (either no input or empty dataset)"));
		return false;
	}

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

	Context->NumTargets = Context->Targets->Lines.Num();

	PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->LookAtUpGetter = new PCGEx::FLocalVectorGetter();
	if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
	{
		Context->LookAtUpGetter->Capture(Settings->LookAtUpSource);
	}

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, {PCGExFactories::EType::Filter}, false);

	return true;
}

bool FPCGExSampleNearestPolylineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPolylineElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPolyline)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->PointFilterManager)

		if (!Context->AdvancePointsIO())
		{
			Context->Done();
			Context->ExecutionComplete();
		}
		else
		{
			if (!Context->PointFilterFactories.IsEmpty())
			{
				Context->PointFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(Context->CurrentIO);
				Context->PointFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->PointFilterFactories, Context->CurrentIO);

				if (Context->PointFilterManager->PrepareForTesting()) { Context->SetState(PCGExDataFilter::State_PreparingFilters); }
				else { Context->SetState(PCGExDataFilter::State_FilteringPoints); }
			}
			else
			{
				Context->SetState(PCGExMT::State_ProcessingPoints);
			}
		}
	}

	if (Context->IsState(PCGExDataFilter::State_PreparingFilters))
	{
		auto PreparePoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO) { Context->PointFilterManager->PrepareSingle(Index); };

		if (!Context->ProcessCurrentPoints(PreparePoint)) { return false; }

		Context->PointFilterManager->PreparationComplete();

		Context->SetState(PCGExDataFilter::State_FilteringPoints);
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO) { Context->PointFilterManager->Test(PointIndex); };

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			if (Settings->bUseLocalRangeMin)
			{
				if (Context->RangeMinGetter->Grab(PointIO)) { PCGE_LOG(Warning, GraphAndLog, FTEXT("RangeMin metadata missing")); }
			}

			if (Settings->bUseLocalRangeMax)
			{
				if (Context->RangeMaxGetter->Grab(PointIO)) { PCGE_LOG(Warning, GraphAndLog, FTEXT("RangeMax metadata missing")); }
			}

			if (Settings->bWriteLookAtTransform)
			{
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Source &&
					!Context->LookAtUpGetter->Grab(PointIO))
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("LookUp is invalid on source."));
				}
			}

			PointIO.CreateOutKeys();

			PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 TaskIndex, const PCGExData::FPointIO& PointIO)
		{
			if (Context->PointFilterManager && !Context->PointFilterManager->Results[TaskIndex]) { return; }

			const FPCGPoint& SourcePoint = PointIO.GetOutPoint(TaskIndex);

			double RangeMin = FMath::Pow(Context->RangeMinGetter->SafeGet(TaskIndex, Settings->RangeMin), 2);
			double RangeMax = FMath::Pow(Context->RangeMaxGetter->SafeGet(TaskIndex, Settings->RangeMax), 2);

			if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

			TArray<PCGExPolyLine::FSampleInfos> TargetsInfos;
			TargetsInfos.Reserve(Context->NumTargets);

			PCGExPolyLine::FTargetsCompoundInfos TargetsCompoundInfos;

			FVector Origin = SourcePoint.Transform.GetLocation();
			auto ProcessTarget = [&](const FTransform& Transform, const double& Time)
			{
				const FVector ModifiedOrigin = PCGExMath::GetSpatializedCenter(Settings->DistanceSettings, SourcePoint, Origin, Transform.GetLocation());
				const double Dist = FVector::DistSquared(ModifiedOrigin, Transform.GetLocation());

				if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
					Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
				{
					TargetsCompoundInfos.UpdateCompound(PCGExPolyLine::FSampleInfos(Transform, Dist, Time));
					return;
				}

				if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

				const PCGExPolyLine::FSampleInfos& Infos = TargetsInfos.Emplace_GetRef(Transform, Dist, Time);
				TargetsCompoundInfos.UpdateCompound(Infos);
			};

			// First: Sample all possible targets
			if (RangeMax > 0)
			{
				for (PCGExData::FPolyLineIO* Line : Context->Targets->Lines)
				{
					FTransform SampledTransform;
					double Time;
					if (!Line->SampleNearestTransform(Origin, FMath::Sqrt(RangeMax), SampledTransform, Time)) { continue; }
					ProcessTarget(SampledTransform, Time);
				}
			}
			else
			{
				for (PCGExData::FPolyLineIO* Line : Context->Targets->Lines)
				{
					double Time;
					ProcessTarget(Line->SampleNearestTransform(Origin, Time), Time);
				}
			}

			// Compound never got updated, meaning we couldn't find target in range
			if (TargetsCompoundInfos.UpdateCount <= 0)
			{
				double FailSafeDist = FMath::Sqrt(RangeMax);
				PCGEX_OUTPUT_VALUE(Success, TaskIndex, false)
				PCGEX_OUTPUT_VALUE(Transform, TaskIndex, SourcePoint.Transform)
				PCGEX_OUTPUT_VALUE(LookAtTransform, TaskIndex, SourcePoint.Transform)
				PCGEX_OUTPUT_VALUE(Distance, TaskIndex, FailSafeDist)
				PCGEX_OUTPUT_VALUE(SignedDistance, TaskIndex, FailSafeDist)
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

			FVector WeightedUp = Settings->LookAtUpSelection == EPCGExSampleSource::Source ?
				                     Context->LookAtUpGetter->SafeGet(TaskIndex, Context->SafeUpVector) :
				                     Context->SafeUpVector;
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

			WeightedUp.Normalize();

			FVector LookAt = (SourcePoint.Transform.GetLocation() - WeightedTransform.GetLocation()).GetSafeNormal();
			const double WeightedDistance = FVector::Dist(Origin, WeightedTransform.GetLocation());

			PCGEX_OUTPUT_VALUE(Success, TaskIndex, TargetsCompoundInfos.IsValid())
			PCGEX_OUTPUT_VALUE(Transform, TaskIndex, WeightedTransform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, TaskIndex, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
			PCGEX_OUTPUT_VALUE(Distance, TaskIndex, WeightedDistance)
			PCGEX_OUTPUT_VALUE(SignedDistance, TaskIndex, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
			PCGEX_OUTPUT_VALUE(Angle, TaskIndex, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
			PCGEX_OUTPUT_VALUE(Time, TaskIndex, WeightedTime)
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }

		PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_WRITE)
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
