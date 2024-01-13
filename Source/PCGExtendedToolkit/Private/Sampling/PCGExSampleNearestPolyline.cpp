// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPolyline.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPolylineElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

UPCGExSampleNearestPolylineSettings::UPCGExSampleNearestPolylineSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!WeightOverDistance)
	{
		WeightOverDistance = PCGEx::WeightDistributionLinear;
	}
}

TArray<FPCGPinProperties> UPCGExSampleNearestPolylineSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertySourceTargets = PinProperties.Emplace_GetRef(PCGEx::SourceTargetsLabel, EPCGDataType::PolyLine, true, true);

#if WITH_EDITOR
	PinPropertySourceTargets.Tooltip = FTEXT("The point data set to check against.");
#endif

	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestPolylineSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestPolylineSettings::GetPreferredChunkSize() const { return 32; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestPolyline)

FPCGExSampleNearestPolylineContext::~FPCGExSampleNearestPolylineContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_CLEANUP(RangeMinGetter)
	PCGEX_CLEANUP(RangeMaxGetter)
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

	PCGEX_FWD(RangeMin)
	PCGEX_FWD(RangeMax)

	PCGEX_FWD(SignAxis)
	PCGEX_FWD(AngleAxis)
	PCGEX_FWD(AngleRange)

	PCGEX_FWD(SampleMethod)
	PCGEX_FWD(WeightMethod)

	PCGEX_FWD(NormalSource)

	PCGEX_FWD(RangeMin)
	PCGEX_FWD(bUseLocalRangeMin)
	Context->RangeMinGetter.Capture(Settings->LocalRangeMin);

	PCGEX_FWD(RangeMax)
	PCGEX_FWD(bUseLocalRangeMax)
	Context->RangeMaxGetter.Capture(Settings->LocalRangeMax);

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

	return true;
}

bool FPCGExSampleNearestPolylineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPolylineElement::Execute);

	PCGEX_CONTEXT(SampleNearestPolyline)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			if (Context->bUseLocalRangeMin && Context->RangeMinGetter.Bind(PointIO))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("RangeMin metadata missing"));
			}

			if (Context->bUseLocalRangeMax && Context->RangeMaxGetter.Bind(PointIO))
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("RangeMax metadata missing"));
			}

			PointIO.CreateOutKeys();

			PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FSamplePolylineTask>(PointIndex, Context->CurrentIO);
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		PCGEX_FOREACH_FIELD_NEARESTPOLYLINE(PCGEX_OUTPUT_WRITE)
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

bool FSamplePolylineTask::ExecuteTask()
{
	const FPCGExSampleNearestPolylineContext* Context = Manager->GetContext<FPCGExSampleNearestPolylineContext>();


	const FPCGPoint& Point = PointIO->GetOutPoint(TaskIndex);

	double RangeMin = FMath::Pow(Context->RangeMinGetter.SafeGet(TaskIndex, Context->RangeMin), 2);
	double RangeMax = FMath::Pow(Context->RangeMaxGetter.SafeGet(TaskIndex, Context->RangeMax), 2);

	if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

	TArray<PCGExPolyLine::FSampleInfos> TargetsInfos;
	TargetsInfos.Reserve(Context->NumTargets);

	PCGExPolyLine::FTargetsCompoundInfos TargetsCompoundInfos;

	FVector Origin = Point.Transform.GetLocation();
	auto ProcessTarget = [&](const FTransform& Transform, const double& Time)
	{
		const double dist = FVector::DistSquared(Origin, Transform.GetLocation());

		if (RangeMax > 0 && (dist < RangeMin || dist > RangeMax)) { return; }

		if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			TargetsCompoundInfos.UpdateCompound(PCGExPolyLine::FSampleInfos(Transform, dist, Time));
		}
		else
		{
			const PCGExPolyLine::FSampleInfos& Infos = TargetsInfos.Emplace_GetRef(Transform, dist, Time);
			TargetsCompoundInfos.UpdateCompound(Infos);
		}
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
		PCGEX_OUTPUT_VALUE(Success, TaskIndex, false)
		return false;
	}

	// Compute individual target weight
	if (Context->WeightMethod == EPCGExWeightMethod::FullRange && RangeMax > 0)
	{
		// Reset compounded infos to full range
		TargetsCompoundInfos.SampledRangeMin = RangeMin;
		TargetsCompoundInfos.SampledRangeMax = RangeMax;
		TargetsCompoundInfos.SampledRangeWidth = RangeMax - RangeMin;
	}

	FVector WeightedLocation = FVector::Zero();
	FVector WeightedLookAt = FVector::Zero();
	FVector WeightedNormal = FVector::Zero();
	FVector WeightedSignAxis = FVector::Zero();
	FVector WeightedAngleAxis = FVector::Zero();
	double WeightedTime = 0;
	double TotalWeight = 0;


	auto ProcessTargetInfos = [&](const PCGExPolyLine::FSampleInfos& TargetInfos, const double Weight)
	{
		const FVector TargetLocationOffset = TargetInfos.Transform.GetLocation() - Origin;
		const FQuat Quat = TargetInfos.Transform.GetRotation();
		WeightedLocation += (TargetLocationOffset * Weight); // Relative to origin
		WeightedLookAt += (TargetLocationOffset.GetSafeNormal()) * Weight;
		WeightedNormal += PCGEx::GetDirection(Quat, Context->NormalSource) * Weight; // Use forward as default
		WeightedSignAxis += PCGEx::GetDirection(Quat, Context->SignAxis) * Weight;
		WeightedAngleAxis += PCGEx::GetDirection(Quat, Context->AngleAxis) * Weight;
		WeightedTime += TargetInfos.Time * Weight;
		TotalWeight += Weight;
	};


	if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
		Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
	{
		const PCGExPolyLine::FSampleInfos& TargetInfos = Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
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
		WeightedLocation /= TotalWeight;
		WeightedLookAt /= TotalWeight;
	}

	WeightedLookAt.Normalize();
	WeightedNormal.Normalize();

	const double WeightedDistance = WeightedLocation.Length();

	PCGEX_OUTPUT_VALUE(Success, TaskIndex, TargetsCompoundInfos.IsValid())
	PCGEX_OUTPUT_VALUE(Location, TaskIndex, Origin + WeightedLocation)
	PCGEX_OUTPUT_VALUE(LookAt, TaskIndex, WeightedLookAt)
	PCGEX_OUTPUT_VALUE(Normal, TaskIndex, WeightedNormal)
	PCGEX_OUTPUT_VALUE(Distance, TaskIndex, WeightedDistance)
	PCGEX_OUTPUT_VALUE(SignedDistance, TaskIndex, FMath::Sign(WeightedSignAxis.Dot(WeightedLookAt)) * WeightedDistance)
	PCGEX_OUTPUT_VALUE(Angle, TaskIndex, PCGExSampling::GetAngle(Context->AngleRange, WeightedAngleAxis, WeightedLookAt))
	PCGEX_OUTPUT_VALUE(Time, TaskIndex, WeightedTime)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
