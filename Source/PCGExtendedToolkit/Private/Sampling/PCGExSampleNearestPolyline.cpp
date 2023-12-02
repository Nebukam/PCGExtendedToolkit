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

PCGExIO::EInitMode UPCGExSampleNearestPolylineSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

int32 UPCGExSampleNearestPolylineSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestPolylineSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPolylineElement>(); }

FPCGContext* FPCGExSampleNearestPolylineElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestPolylineContext* Context = new FPCGExSampleNearestPolylineContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestPolylineSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPolylineSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Targets = InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);

	if (!Targets.IsEmpty())
	{
		Context->Targets = NewObject<UPCGExPolyLineIOGroup>();
		Context->Targets->Initialize(Targets);
	}

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	Context->RangeMin = Settings->RangeMin;
	Context->RangeMax = Settings->RangeMax;

	PCGEX_FORWARD_OUT_ATTRIBUTE(Success)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(LookAt)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)
	PCGEX_FORWARD_OUT_ATTRIBUTE(SignedDistance)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Angle)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Time)

	return Context;
}

bool FPCGExSampleNearestPolylineElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleNearestPolylineContext* Context = static_cast<FPCGExSampleNearestPolylineContext*>(InContext);
	const UPCGExSampleNearestPolylineSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPolylineSettings>();
	check(Settings);

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

	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Success)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(LookAt)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(SignedDistance)
	Context->SignAxis = Settings->SignAxis;

	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Angle)
	Context->AngleAxis = Settings->AngleAxis;
	Context->AngleRange = Settings->AngleRange;

	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Time)

	Context->RangeMin = Settings->RangeMin;
	Context->bLocalRangeMin = Settings->bUseLocalRangeMin;
	Context->RangeMinGetter.Capture(Settings->LocalRangeMin);

	Context->RangeMax = Settings->RangeMax;
	Context->bLocalRangeMax = Settings->bUseLocalRangeMax;
	Context->RangeMaxGetter.Capture(Settings->LocalRangeMax);

	Context->SampleMethod = Settings->SampleMethod;
	Context->WeightMethod = Settings->WeightMethod;

	Context->NormalSource = Settings->NormalSource;

	Context->NumTargets = Context->Targets->PolyLines.Num();

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
		auto Initialize = [&](UPCGExPointIO* PointIO)
		{
			PointIO->BuildMetadataEntries();

			if (Context->bLocalRangeMin)
			{
				if (Context->RangeMinGetter.Validate(PointIO->Out))
				{
					PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMin", "RangeMin metadata missing"));
				}
			}

			if (Context->bLocalRangeMax)
			{
				if (Context->RangeMaxGetter.Validate(PointIO->Out))
				{
					PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMax", "RangeMax metadata missing"));
				}
			}

			PCGEX_INIT_ATTRIBUTE_OUT(Success, bool)
			PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(LookAt, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
			PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
			PCGEX_INIT_ATTRIBUTE_OUT(SignedDistance, double)
			PCGEX_INIT_ATTRIBUTE_OUT(Angle, double)
			PCGEX_INIT_ATTRIBUTE_OUT(Time, double)
		};

		auto ProcessPoint = [&](const int32 PointIndex, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& Point = PointIO->GetOutPoint(PointIndex);

			double RangeMin = FMath::Pow(Context->RangeMinGetter.GetValueSafe(Point, Context->RangeMin), 2);
			double RangeMax = FMath::Pow(Context->RangeMaxGetter.GetValueSafe(Point, Context->RangeMax), 2);

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
				for (UPCGExPolyLineIO* Line : Context->Targets->PolyLines)
				{
					FTransform SampledTransform;
					double Time;
					if (!Line->SampleNearestTransform(Origin, FMath::Sqrt(RangeMax), SampledTransform, Time)) { continue; }
					ProcessTarget(SampledTransform, Time);
				}
			}
			else
			{
				for (UPCGExPolyLineIO* Line : Context->Targets->PolyLines)
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

			double OutAngle = 0;
			if (Context->bWriteAngle)
			{
				PCGExSampling::GetAngle(Context->AngleRange, WeightedAngleAxis, WeightedLookAt, OutAngle);
			}

			const PCGMetadataEntryKey Key = Point.MetadataEntry;
			PCGEX_SET_OUT_ATTRIBUTE(Success, Key, TargetsCompoundInfos.IsValid())
			PCGEX_SET_OUT_ATTRIBUTE(Location, Key, Origin + WeightedLocation)
			PCGEX_SET_OUT_ATTRIBUTE(LookAt, Key, WeightedLookAt)
			PCGEX_SET_OUT_ATTRIBUTE(Normal, Key, WeightedNormal)
			PCGEX_SET_OUT_ATTRIBUTE(Distance, Key, WeightedDistance)
			PCGEX_SET_OUT_ATTRIBUTE(SignedDistance, Key, FMath::Sign(WeightedSignAxis.Dot(WeightedLookAt)) * WeightedDistance)
			PCGEX_SET_OUT_ATTRIBUTE(Angle, Key, OutAngle)
			PCGEX_SET_OUT_ATTRIBUTE(Time, Key, WeightedTime)
		};


		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->Targets->Flush();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
