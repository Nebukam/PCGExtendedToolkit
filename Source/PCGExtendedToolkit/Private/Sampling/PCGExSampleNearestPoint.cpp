// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"

#include "PCGExFactoryProvider.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Graph/PCGExCluster.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"
#define PCGEX_NAMESPACE SampleNearestPoint

UPCGExSampleNearestPointSettings::UPCGExSampleNearestPointSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "The point data set to check against.", Required, {})
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestPointSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestPointSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestPoint)

FPCGExSampleNearestPointContext::~FPCGExSampleNearestPointContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PointFilterManager)
	PointFilterFactories.Empty();
	PCGEX_DELETE(ValueFilterManager)
	ValueFilterFactories.Empty();

	PCGEX_DELETE(RangeMinGetter)
	PCGEX_DELETE(RangeMaxGetter)
	PCGEX_DELETE(LookAtUpGetter)

	PCGEX_DELETE(Targets)

	PCGEX_DELETE(Blender)

	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_DELETE)
}

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
			if (SpatialData->ToPointData(Context))
			{
				Context->Targets = PCGExData::PCGExPointIO::GetPointIO(Context, Target);
			}
		}
	}

	if (!Context->Targets || Context->Targets->GetNum() < 1)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("No targets (either no input or empty dataset)"));
		return false;
	}

	TSet<FName> MissingTargetAttributes;
	PCGExDataBlending::AssembleBlendingSettings(
		Settings->bBlendPointProperties ? Settings->PointPropertiesBlendingSettings : FPCGExPropertiesBlendingSettings(EPCGExDataBlendingType::None),
		Settings->TargetAttributes, *Context->Targets, Context->BlendingSettings, MissingTargetAttributes);

	for (const FName Id : MissingTargetAttributes) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on edges: {0}."), FText::FromName(Id))); }

	if (!Context->BlendingSettings.FilteredAttributes.IsEmpty() ||
		!Context->BlendingSettings.GetPropertiesBlendingSettings().HasNoBlending())
	{
		Context->Blender = new PCGExDataBlending::FMetadataBlender(&Context->BlendingSettings);
	}

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	Context->RangeMinGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->RangeMinGetter->Capture(Settings->LocalRangeMin);

	Context->RangeMaxGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->RangeMaxGetter->Capture(Settings->LocalRangeMax);

	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_FWD)

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->LookAtUpGetter = new PCGEx::FLocalVectorGetter();
	Context->LookAtUpGetter->Capture(Settings->LookAtUpSource);

	if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
	{
		if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
		{
			if (!Context->LookAtUpGetter->Grab(*Context->Targets)) { PCGE_LOG(Warning, GraphAndLog, FTEXT("LookUp is invalid on target.")); }
		}
	}

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, {PCGExFactories::EType::Filter}, false);
	PCGExFactories::GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, Context->ValueFilterFactories, {PCGExFactories::EType::Filter}, false);

	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->CurrentIO->CreateOutKeys();
			Context->Targets->CreateInKeys();

			if (Context->Blender) { Context->Blender->PrepareForData(*Context->CurrentIO, *Context->Targets); }

			if (!Context->PointFilterFactories.IsEmpty() || !Context->ValueFilterFactories.IsEmpty())
			{
				Context->SetState(PCGExDataFilter::State_FilteringPoints);
			}
			else
			{
				Context->SetState(PCGExMT::State_ProcessingPoints);
			}
		}
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			PCGEX_DELETE(Context->PointFilterManager)
			PCGEX_DELETE(Context->ValueFilterManager)

			if (!Context->PointFilterFactories.IsEmpty())
			{
				Context->PointFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(&PointIO);
				Context->PointFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->PointFilterFactories, &PointIO);
			}
			if (!Context->ValueFilterFactories.IsEmpty())
			{
				Context->ValueFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(&PointIO);
				Context->ValueFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->ValueFilterFactories, &PointIO);
			}
		};

		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			if (Context->PointFilterManager) { Context->PointFilterManager->Test(PointIndex); }
			if (Context->ValueFilterManager) { Context->ValueFilterManager->Test(PointIndex); }
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }

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

			PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 ReadIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FPCGExSamplePointTask>(ReadIndex, const_cast<PCGExData::FPointIO*>(&PointIO));
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		if (Context->Blender) { Context->Blender->Write(); }

		PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_WRITE)
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

bool FPCGExSamplePointTask::ExecuteTask()
{
	const FPCGExSampleNearestPointContext* Context = Manager->GetContext<FPCGExSampleNearestPointContext>();
	PCGEX_SETTINGS(SampleNearestPoint)

	if (Context->PointFilterManager && !Context->PointFilterManager->Results[TaskIndex]) { return false; }

	const bool bSingleSample = (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget);

	const TArray<FPCGPoint>& TargetPoints = Context->Targets->GetIn()->GetPoints();
	const int32 NumTargets = TargetPoints.Num();
	FPCGPoint& SourcePoint = PointIO->GetMutablePoint(TaskIndex);
	const FVector SourceCenter = SourcePoint.Transform.GetLocation();

	double RangeMin = FMath::Pow(Context->RangeMinGetter->SafeGet(TaskIndex, Settings->RangeMin), 2);
	double RangeMax = FMath::Pow(Context->RangeMaxGetter->SafeGet(TaskIndex, Settings->RangeMax), 2);

	if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

	TArray<PCGExNearestPoint::FTargetInfos> TargetsInfos;
	TargetsInfos.Reserve(Context->Targets->GetNum());

	PCGExNearestPoint::FTargetsCompoundInfos TargetsCompoundInfos;
	auto ProcessTarget = [&](const int32 PointIndex, const FPCGPoint& Target)
	{
		if (Context->ValueFilterManager && !Context->ValueFilterManager->Results[PointIndex]) { return; }

		FVector A;
		FVector B;

		Settings->DistanceSettings.GetCenters(SourcePoint, Target, A, B);

		const double Dist = FVector::DistSquared(A, B);

		if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

		if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
			Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
		{
			TargetsCompoundInfos.UpdateCompound(PCGExNearestPoint::FTargetInfos(PointIndex, Dist));
		}
		else
		{
			const PCGExNearestPoint::FTargetInfos& Infos = TargetsInfos.Emplace_GetRef(PointIndex, Dist);
			TargetsCompoundInfos.UpdateCompound(Infos);
		}
	};

	if (RangeMax > 0)
	{
		const FBox Box = FBoxCenterAndExtent(SourceCenter, FVector(FMath::Sqrt(RangeMax))).GetBox();
		auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
		{
			const ptrdiff_t PointIndex = InPointRef.Point - TargetPoints.GetData();
			if (!TargetPoints.IsValidIndex(PointIndex)) { return; }

			ProcessTarget(PointIndex, TargetPoints[PointIndex]);
		};

		const UPCGPointData::PointOctree& Octree = Context->Targets->GetIn()->GetOctree();
		Octree.FindElementsWithBoundsTest(Box, ProcessNeighbor);
	}
	else
	{
		for (int i = 0; i < NumTargets; i++) { ProcessTarget(i, TargetPoints[i]); }
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
		PCGEX_OUTPUT_VALUE(NumSamples, TaskIndex, 0)
		return false;
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
	double TotalWeight = 0;


	auto ProcessTargetInfos = [&]
		(const PCGExNearestPoint::FTargetInfos& TargetInfos, const double Weight)
	{
		const FPCGPoint& Target = Context->Targets->GetInPoint(TargetInfos.Index);

		WeightedTransform.SetRotation(WeightedTransform.GetRotation() + (Target.Transform.GetRotation() * Weight));
		WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() + (Target.Transform.GetScale3D() * Weight));
		WeightedTransform.SetLocation(WeightedTransform.GetLocation() + (Target.Transform.GetLocation() * Weight));

		if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += Context->LookAtUpGetter->SafeGet(TargetInfos.Index, Context->SafeUpVector) * Weight; }

		WeightedSignAxis += PCGExMath::GetDirection(Target.Transform.GetRotation(), Settings->SignAxis) * Weight;
		WeightedAngleAxis += PCGExMath::GetDirection(Target.Transform.GetRotation(), Settings->AngleAxis) * Weight;

		TotalWeight += Weight;

		if (Context->Blender) { Context->Blender->Blend(TaskIndex, TargetInfos.Index, TaskIndex, Weight); }
	};

	if (Context->Blender) { Context->Blender->PrepareForBlending(TaskIndex, &SourcePoint); }

	if (bSingleSample)
	{
		const PCGExNearestPoint::FTargetInfos& TargetInfos = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
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

	double Count = bSingleSample ? 1 : TargetsInfos.Num();

	if (Context->Blender) { Context->Blender->CompleteBlending(TaskIndex, Count, TotalWeight); }

	if (TotalWeight != 0) // Dodge NaN
	{
		WeightedUp /= TotalWeight;

		WeightedTransform.SetRotation(WeightedTransform.GetRotation() / TotalWeight);
		WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() / TotalWeight);
		WeightedTransform.SetLocation(WeightedTransform.GetLocation() / TotalWeight);
	}

	WeightedUp.Normalize();

	FVector LookAt = (SourcePoint.Transform.GetLocation() - WeightedTransform.GetLocation()).GetSafeNormal();
	const double WeightedDistance = FVector::Dist(SourcePoint.Transform.GetLocation(), WeightedTransform.GetLocation());

	PCGEX_OUTPUT_VALUE(Success, TaskIndex, TargetsCompoundInfos.IsValid())
	PCGEX_OUTPUT_VALUE(Transform, TaskIndex, WeightedTransform)
	PCGEX_OUTPUT_VALUE(LookAtTransform, TaskIndex, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
	PCGEX_OUTPUT_VALUE(Distance, TaskIndex, WeightedDistance)
	PCGEX_OUTPUT_VALUE(SignedDistance, TaskIndex, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
	PCGEX_OUTPUT_VALUE(Angle, TaskIndex, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
	PCGEX_OUTPUT_VALUE(NumSamples, TaskIndex, Count)

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
