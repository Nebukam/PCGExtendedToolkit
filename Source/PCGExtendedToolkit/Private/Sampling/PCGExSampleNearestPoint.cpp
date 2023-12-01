// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"

#include "PCGExPointsProcessor.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"

UPCGExSampleNearestPointSettings::UPCGExSampleNearestPointSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (NormalSource.Selector.GetName() == FName("@Last"))
	{
		NormalSource.Selector.Update(TEXT("$Transform"));
	}

	if (!WeightOverDistance)
	{
		WeightOverDistance = PCGEx::WeightDistributionLinear;
	}
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinPropertySourceTargets = PinProperties.Emplace_GetRef(PCGEx::SourceTargetsLabel, EPCGDataType::Point, false, false);

#if WITH_EDITOR
	PinPropertySourceTargets.Tooltip = LOCTEXT("PCGExSourceTargetsPointsPinTooltip", "The point data set to check against.");
#endif // WITH_EDITOR

	return PinProperties;
}

PCGExIO::EInitMode UPCGExSampleNearestPointSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

int32 UPCGExSampleNearestPointSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExSampleNearestPointSettings::CreateElement() const { return MakeShared<FPCGExSampleNearestPointElement>(); }

FPCGContext* FPCGExSampleNearestPointElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSampleNearestPointContext* Context = new FPCGExSampleNearestPointContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExSampleNearestPointSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPointSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Targets = InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);
	if (!Targets.IsEmpty())
	{
		const FPCGTaggedData& Target = Targets[0];
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Target.Data))
		{
			if (const UPCGPointData* PointData = SpatialData->ToPointData(Context))
			{
				Context->Targets = const_cast<UPCGPointData*>(PointData);
				Context->NumTargets = Context->Targets->GetPoints().Num();
			}
		}
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

	return Context;
}

bool FPCGExSampleNearestPointElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);
	const UPCGExSampleNearestPointSettings* Settings = Context->GetInputSettings<UPCGExSampleNearestPointSettings>();
	check(Settings);

	if (!Context->Targets || Context->NumTargets < 1)
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

	Context->RangeMin = Settings->RangeMin;
	Context->bLocalRangeMin = Settings->bUseLocalRangeMin;
	Context->RangeMinGetter.Capture(Settings->LocalRangeMin);

	Context->RangeMax = Settings->RangeMax;
	Context->bLocalRangeMax = Settings->bUseLocalRangeMax;
	Context->RangeMaxGetter.Capture(Settings->LocalRangeMax);

	Context->SampleMethod = Settings->SampleMethod;
	Context->WeightMethod = Settings->WeightMethod;

	if (Context->bWriteNormal)
	{
		Context->NormalInput.Capture(Settings->NormalSource);
		if (!Context->NormalInput.Validate(Context->Targets))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidNormalSource", "Normal source is invalid."));
		}
	}

	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	FPCGExSampleNearestPointContext* Context = static_cast<FPCGExSampleNearestPointContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }

		// Ensure targets have metadata keys.
		Context->TargetsCache = const_cast<UPCGPointData*>(Context->Targets->DuplicateData(true)->ToPointData(Context));
		Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->TargetsCache->GetOctree()));
		TArray<FPCGPoint>& TargetPoints = Context->TargetsCache->GetMutablePoints();
		const int32 NumTargets = TargetPoints.Num();
		Context->TargetIndices.Reserve(NumTargets);

		int32 i = 0;
		for (FPCGPoint& Pt : TargetPoints)
		{
			PCGMetadataEntryKey& Key = Pt.MetadataEntry;
			Context->TargetsCache->Metadata->InitializeOnSet(Key);
			Context->TargetIndices.Add(Key, i);
			i++;
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::State_Done);
		}
		else
		{
			Context->SetState(PCGExMT::State_ProcessingPoints);
		}
	}

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
	};

	auto ProcessPoint = [&](const int32 ReadIndex, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetOutPoint(ReadIndex);

		double RangeMin = FMath::Pow(Context->RangeMinGetter.GetValueSafe(Point, Context->RangeMin), 2);
		double RangeMax = FMath::Pow(Context->RangeMaxGetter.GetValueSafe(Point, Context->RangeMax), 2);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExNearestPoint::FTargetInfos> TargetsInfos;
		TargetsInfos.Reserve(Context->NumTargets);

		PCGExNearestPoint::FTargetsCompoundInfos TargetsCompoundInfos;

		FVector Origin = Point.Transform.GetLocation();
		auto ProcessTarget = [&](const FPCGPoint& Target)
		{
			const double dist = FVector::DistSquared(Origin, Target.Transform.GetLocation());

			if (RangeMax > 0 && (dist < RangeMin || dist > RangeMax)) { return; }

			if (Context->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
				Context->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				FReadScopeLock ReadLock(Context->IndicesLock);
				TargetsCompoundInfos.UpdateCompound(PCGExNearestPoint::FTargetInfos(*Context->TargetIndices.Find(Target.MetadataEntry), dist));
			}
			else
			{
				FReadScopeLock ReadLock(Context->IndicesLock);
				const PCGExNearestPoint::FTargetInfos& Infos = TargetsInfos.Emplace_GetRef(*Context->TargetIndices.Find(Target.MetadataEntry), dist);
				TargetsCompoundInfos.UpdateCompound(Infos);
			}
		};

		// First: Sample all possible targets
		if (RangeMax > 0)
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(FMath::Sqrt(RangeMax)));
			Context->Octree->FindElementsWithBoundsTest(
				Box,
				[&ProcessTarget](const FPCGPointRef& TargetPointRef)
				{
					const FPCGPoint* TargetPoint = TargetPointRef.Point;
					ProcessTarget(*TargetPoint);
				});
		}
		else
		{
			const TArray<FPCGPoint>& Targets = Context->TargetsCache->GetPoints();
			for (const FPCGPoint& TargetPoint : Targets) { ProcessTarget(TargetPoint); }
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
		double TotalWeight = 0;


		auto ProcessTargetInfos = [&]
			(const PCGExNearestPoint::FTargetInfos& TargetInfos, double Weight)
		{
			const FPCGPoint& TargetPoint = Context->TargetsCache->GetPoints()[TargetInfos.Index];
			const FVector TargetLocationOffset = TargetPoint.Transform.GetLocation() - Origin;
			WeightedLocation += (TargetLocationOffset * Weight); // Relative to origin
			WeightedLookAt += (TargetLocationOffset.GetSafeNormal()) * Weight;
			WeightedNormal += Context->NormalInput.GetValue(TargetPoint) * Weight;
			WeightedSignAxis += PCGEx::GetDirection(TargetPoint.Transform.GetRotation(), Context->SignAxis) * Weight;
			WeightedAngleAxis += PCGEx::GetDirection(TargetPoint.Transform.GetRotation(), Context->AngleAxis) * Weight;

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
	};

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->TargetsCache->GetMutablePoints().Empty();
		Context->TargetsCache = nullptr;
		Context->TargetIndices.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
