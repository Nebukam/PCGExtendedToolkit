// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExSampleNearestPolyline.h"

#include "PCGExCommon.h"
#include "PCGPin.h"
#include <algorithm>

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

PCGEx::EIOInit UPCGExSampleNearestPolylineSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

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

	Context->RangeMin = Settings->MaxDistance;
	Context->bUseOctree = Settings->MaxDistance <= 0;

	PCGEX_FORWARD_OUT_ATTRIBUTE(Location)
	PCGEX_FORWARD_OUT_ATTRIBUTE(LookAt)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Normal)
	PCGEX_FORWARD_OUT_ATTRIBUTE(Distance)
	PCGEX_FORWARD_OUT_ATTRIBUTE(SignedDistance)

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

	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Location)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(LookAt)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Normal)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(Distance)
	PCGEX_CHECK_OUT_ATTRIBUTE_NAME(SignedDistance)

	Context->RangeMin = Settings->RangeMin;
	Context->bLocalRangeMin = Settings->bUseLocalRangeMin;
	Context->RangeMinInput.Capture(Settings->LocalRangeMin);

	Context->RangeMax = Settings->RangeMax;
	Context->bLocalRangeMax = Settings->bUseLocalRangeMax;
	Context->RangeMaxInput.Capture(Settings->LocalRangeMax);

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

	if (Context->IsState(PCGExMT::EState::Setup))
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto InitializeForIO = [&Context, this](UPCGExPointIO* IO)
	{
		IO->BuildMetadataEntries();

		if (Context->bLocalRangeMin)
		{
			if (Context->RangeMinInput.Validate(IO->Out))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMin", "RangeMin metadata missing"));
			}
		}

		if (Context->bLocalRangeMax)
		{
			if (Context->RangeMaxInput.Validate(IO->Out))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidLocalRangeMax", "RangeMax metadata missing"));
			}
		}

		PCGEX_INIT_ATTRIBUTE_OUT(Location, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(LookAt, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Normal, FVector)
		PCGEX_INIT_ATTRIBUTE_OUT(Distance, double)
		PCGEX_INIT_ATTRIBUTE_OUT(SignedDistance, double)
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 ReadIndex, const UPCGExPointIO* IO)
	{
		double RangeMin = FMath::Pow(Context->RangeMinInput.GetValueSafe(Point, Context->RangeMin), 2);
		double RangeMax = FMath::Pow(Context->RangeMaxInput.GetValueSafe(Point, Context->RangeMax), 2);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExPolyLine::FSampleInfos> TargetsInfos;
		TargetsInfos.Reserve(Context->NumTargets);

		PCGExPolyLine::FTargetsCompoundInfos TargetsCompoundInfos;

		FVector Origin = Point.Transform.GetLocation();
		auto ProcessTarget = [&Context, &Origin, &ReadIndex, &RangeMin, &RangeMax, &TargetsInfos, &TargetsCompoundInfos](const FTransform& Transform)
		{
			const double dist = FVector::DistSquared(Origin, Transform.GetLocation());

			if (RangeMax > 0 && (dist < RangeMin || dist > RangeMax)) { return; }

			if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
				Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				TargetsCompoundInfos.UpdateCompound(PCGExPolyLine::FSampleInfos(Transform, dist));
			}
			else
			{
				const PCGExPolyLine::FSampleInfos& Infos = TargetsInfos.Emplace_GetRef(Transform, dist);
				TargetsCompoundInfos.UpdateCompound(Infos);
			}
		};

		// First: Sample all possible targets
		if (RangeMax > 0)
		{
			for (UPCGExPolyLineIO* Line : Context->Targets->PolyLines)
			{
				FTransform SampledTransform;
				if (!Line->SampleNearestTransformWithinRange(Origin, FMath::Sqrt(RangeMax), SampledTransform)) { continue; }
				ProcessTarget(SampledTransform);
			}
		}
		else
		{
			for (UPCGExPolyLineIO* Line : Context->Targets->PolyLines)
			{
				ProcessTarget(Line->SampleNearestTransform(Origin));
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
		double WeightedDistance = 0;
		double TotalWeight = 0;


		auto ProcessTargetInfos = [&Context, &Origin, &WeightedLocation, &WeightedLookAt, &WeightedNormal, &TotalWeight]
			(const PCGExPolyLine::FSampleInfos& TargetInfos, double Weight)
		{
			const FVector TargetLocationOffset = TargetInfos.Transform.GetLocation() - Origin;
			WeightedLocation += (TargetLocationOffset * Weight); // Relative to origin
			WeightedLookAt += (TargetLocationOffset.GetSafeNormal()) * Weight;
			WeightedNormal += PCGEx::Common::GetDirection(TargetInfos.Transform.GetRotation(), Context->NormalSource) * Weight; // Use forward as default

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

		const PCGMetadataEntryKey Key = Point.MetadataEntry;
		PCGEX_SET_OUT_ATTRIBUTE(Location, Key, Origin + WeightedLocation)
		PCGEX_SET_OUT_ATTRIBUTE(LookAt, Key, WeightedLookAt)
		PCGEX_SET_OUT_ATTRIBUTE(Normal, Key, WeightedNormal)
		//PCGEX_SET_OUT_ATTRIBUTE(SignedDistance, Key, WeightedNormal)
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
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
