// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPolyline.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPolylineElement"

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
	PinPropertySourceTargets.Tooltip = LOCTEXT("PCGExSourceTargetsPointsPinTooltip", "The point data set to check against.");
#endif // WITH_EDITOR

	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestPolylineSettings::GetPointOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestPolylineSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestPolylineSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPolylineElement>(); }

FPCGExSampleNearestPolylineContext::~FPCGExSampleNearestPolylineContext()
{
	PCGEX_DELETE(Targets)
	PCGEX_SAMPLENEARESTPOLYLINE_FOREACH(PCGEX_OUTPUT_DELETE)
}

FPCGContext* FPCGExSampleNearestPolylineElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestPolylineContext* Context = new FPCGExSampleNearestPolylineContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	PCGEX_SETTINGS(UPCGExSampleNearestPolylineSettings)

	TArray<FPCGTaggedData> Targets = InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);

	if (!Targets.IsEmpty())
	{
		Context->Targets = new PCGExData::FPolyLineIOGroup(Targets);
	}

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

	Context->NumTargets = Context->Targets->Lines.Num();

	PCGEX_SAMPLENEARESTPOLYLINE_FOREACH(PCGEX_OUTPUT_FWD)

	return Context;
}

bool FPCGExSampleNearestPolylineElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	PCGEX_CONTEXT(FPCGExSampleNearestPolylineContext)
	PCGEX_SETTINGS(UPCGExSampleNearestPolylineSettings)

	if (!Context->Targets || Context->Targets->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingTargets", "No targets (either no input or empty dataset)"));
		return false;
	}

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidWeightCurve", "Weight Curve asset could not be loaded."));
		return false;
	}

	PCGEX_SAMPLENEARESTPOLYLINE_FOREACH(PCGEX_OUTPUT_VALIDATE_NAME)

	return true;
}

bool FPCGExSampleNearestPolylineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPolylineElement::Execute);

	FPCGExSampleNearestPolylineContext* Context = static_cast<FPCGExSampleNearestPolylineContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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
					PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMin", "RangeMin metadata missing"));
				}
			}

			if (Context->bUseLocalRangeMax)
			{
				if (Context->RangeMaxGetter.Bind(PointIO))
				{
					PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMax", "RangeMax metadata missing"));
				}
			}

			PCGEX_SAMPLENEARESTPOLYLINE_FOREACH(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetOutPoint(PointIndex);

			double RangeMin = FMath::Pow(Context->RangeMinGetter.GetValueSafe(PointIndex, Context->RangeMin), 2);
			double RangeMax = FMath::Pow(Context->RangeMaxGetter.GetValueSafe(PointIndex, Context->RangeMax), 2);

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
			if (TargetsCompoundInfos.UpdateCount <= 0) { return; }

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

			PCGEX_OUTPUT_VALUE(Success, PointIndex, TargetsCompoundInfos.IsValid())
			PCGEX_OUTPUT_VALUE(Location, PointIndex, Origin + WeightedLocation)
			PCGEX_OUTPUT_VALUE(LookAt, PointIndex, WeightedLookAt)
			PCGEX_OUTPUT_VALUE(Normal, PointIndex, WeightedNormal)
			PCGEX_OUTPUT_VALUE(Distance, PointIndex, WeightedDistance)
			PCGEX_OUTPUT_VALUE(SignedDistance, PointIndex, FMath::Sign(WeightedSignAxis.Dot(WeightedLookAt)) * WeightedDistance)
			PCGEX_OUTPUT_VALUE(Angle, PointIndex, PCGExSampling::GetAngle(Context->AngleRange, WeightedAngleAxis, WeightedLookAt))
			PCGEX_OUTPUT_VALUE(Time, PointIndex, WeightedTime)
		};


		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			PCGEX_SAMPLENEARESTPOLYLINE_FOREACH(PCGEX_OUTPUT_WRITE)
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
