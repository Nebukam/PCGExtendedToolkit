// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"
#define PCGEX_NAMESPACE SampleNearestPoint

UPCGExSampleNearestPointSettings::UPCGExSampleNearestPointSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (NormalSource.Selector.GetName() == FName("@Last")) { NormalSource.Selector.Update(TEXT("$Transform")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinear; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertySourceTargets = PinProperties.Emplace_GetRef(PCGEx::SourceTargetsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySourceTargets.Tooltip = FTEXT("The point data set to check against.");
#endif // WITH_EDITOR

	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestPointSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestPointSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestPointSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPointElement>(); }

FPCGExSampleNearestPointContext::~FPCGExSampleNearestPointContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_CLEANUP(RangeMinGetter)
	PCGEX_CLEANUP(RangeMaxGetter)
	PCGEX_CLEANUP(NormalGetter)

	PCGEX_DELETE(Targets)

	PCGEX_SAMPLENEARESTPOINT_FOREACH(PCGEX_OUTPUT_DELETE)
}

PCGEX_INITIALIZE_CONTEXT(SampleNearestPoint)

bool FPCGExSampleNearestPointElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);
	if (!Targets.IsEmpty())
	{
		const FPCGTaggedData& Target = Targets[0];
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Target.Data))
		{
			if (const UPCGPointData* PointData = SpatialData->ToPointData(Context))
			{
				Context->Targets = PCGExData::PCGExPointIO::GetPointIO(Context, Target);
			}
		}
	}

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	PCGEX_FWD(RangeMin)
	PCGEX_FWD(RangeMax)

	PCGEX_FWD(SignAxis)

	PCGEX_FWD(AngleAxis)
	PCGEX_FWD(AngleRange)

	PCGEX_FWD(RangeMin)
	PCGEX_FWD(bUseLocalRangeMin)
	Context->RangeMinGetter.Capture(Settings->LocalRangeMin);

	PCGEX_FWD(RangeMax)
	PCGEX_FWD(bUseLocalRangeMax)
	Context->RangeMaxGetter.Capture(Settings->LocalRangeMax);

	PCGEX_FWD(SampleMethod)
	PCGEX_FWD(WeightMethod)

	PCGEX_SAMPLENEARESTPOINT_FOREACH(PCGEX_OUTPUT_FWD)

	if (!Context->Targets || Context->Targets->GetNum() < 1)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No targets (either no input or empty dataset)"));
		return false;
	}

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

	PCGEX_SAMPLENEARESTPOINT_FOREACH(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Context->NormalWriter)
	{
		Context->NormalGetter.Capture(Settings->NormalSource);
		if (!Context->NormalGetter.Bind(*Context->Targets))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Normal source is invalid."));
		}
	}

	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	PCGEX_CONTEXT(SampleNearestPoint)

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
			if (Context->bUseLocalRangeMin)
			{
				if (Context->RangeMinGetter.Bind(PointIO))
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("RangeMin metadata missing"));
				}
			}

			if (Context->bUseLocalRangeMax)
			{
				if (Context->RangeMaxGetter.Bind(PointIO))
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("RangeMax metadata missing"));
				}
			}

			PointIO.CreateOutKeys();
			PCGEX_SAMPLENEARESTPOINT_FOREACH(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 ReadIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FSamplePointTask>(ReadIndex, const_cast<PCGExData::FPointIO*>(&PointIO));
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork); }
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (Context->IsAsyncWorkComplete())
		{
			PCGEX_SAMPLENEARESTPOINT_FOREACH(PCGEX_OUTPUT_WRITE)
			Context->CurrentIO->OutputTo(Context);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

bool FSamplePointTask::ExecuteTask()
{
	const FPCGExSampleNearestPointContext* Context = Manager->GetContext<FPCGExSampleNearestPointContext>();
	PCGEX_ASYNC_CHECKPOINT

	const int32 NumTargets = Context->Targets->GetNum();
	const FVector Origin = PointIO->GetOutPoint(TaskIndex).Transform.GetLocation();

	double RangeMin = FMath::Pow(Context->RangeMinGetter.SafeGet(TaskIndex, Context->RangeMin), 2);
	double RangeMax = FMath::Pow(Context->RangeMaxGetter.SafeGet(TaskIndex, Context->RangeMax), 2);

	if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

	TArray<PCGExNearestPoint::FTargetInfos> TargetsInfos;
	TargetsInfos.Reserve(Context->Targets->GetNum());

	PCGExNearestPoint::FTargetsCompoundInfos TargetsCompoundInfos;
	auto ProcessTarget = [&](const PCGEx::FPointRef& Target)
	{
		const double Dist = FVector::DistSquared(Origin, Target.Point->Transform.GetLocation());

		if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

		if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			TargetsCompoundInfos.UpdateCompound(PCGExNearestPoint::FTargetInfos(Target.Index, Dist));
		}
		else
		{
			const PCGExNearestPoint::FTargetInfos& Infos = TargetsInfos.Emplace_GetRef(Target.Index, Dist);
			TargetsCompoundInfos.UpdateCompound(Infos);
		}
	};

	if (RangeMax > 0)
	{
		const FBox Box = FBoxCenterAndExtent(Origin, FVector(FMath::Sqrt(RangeMax))).GetBox();
		for (int i = 0; i < NumTargets; i++)
		{
			const FPCGPoint& Target = Context->Targets->GetInPoint(i);
			//TODO: Target locations can be cached
			if (Box.IsInside(Target.Transform.GetLocation())) { ProcessTarget(PCGEx::FPointRef(Target, i)); }
		}
	}
	else
	{
		for (int i = 0; i < NumTargets; i++) { ProcessTarget(Context->Targets->GetInPointRef(i)); }
	}

	// Compound never got updated, meaning we couldn't find target in range
	if (TargetsCompoundInfos.UpdateCount <= 0) { return false; }

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
	double TotalWeight = 0;


	auto ProcessTargetInfos = [&]
		(const PCGExNearestPoint::FTargetInfos& TargetInfos, double Weight)
	{
		const FPCGPoint& Target = Context->Targets->GetInPoint(TargetInfos.Index);
		const FVector TargetLocationOffset = Target.Transform.GetLocation() - Origin;

		WeightedLocation += (TargetLocationOffset * Weight); // Relative to origin
		WeightedLookAt += (TargetLocationOffset.GetSafeNormal()) * Weight;
		WeightedNormal += Context->NormalGetter[TargetInfos.Index] * Weight;
		WeightedSignAxis += PCGEx::GetDirection(Target.Transform.GetRotation(), Context->SignAxis) * Weight;
		WeightedAngleAxis += PCGEx::GetDirection(Target.Transform.GetRotation(), Context->AngleAxis) * Weight;

		TotalWeight += Weight;
	};

	if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
		Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
	{
		const PCGExNearestPoint::FTargetInfos& TargetInfos = Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
		const double Weight = Context->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
		ProcessTargetInfos(TargetInfos, Weight);
	}
	else
	{
		for (PCGExNearestPoint::FTargetInfos& TargetInfos : TargetsInfos)
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

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
