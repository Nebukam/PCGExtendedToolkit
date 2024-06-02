// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleProjectedNearestPoint.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "Data/Blending/PCGExPropertiesBlender.h"
#include "Sampling/PCGExSampleNearestPoint.h"

#define LOCTEXT_NAMESPACE "PCGExSampleProjectedNearestPointElement"
#define PCGEX_NAMESPACE SampleProjectedNearestPoint

UPCGExSampleProjectedNearestPointSettings::UPCGExSampleProjectedNearestPointSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleProjectedNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "The point data set to check against.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleProjectedNearestPointSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleProjectedNearestPointSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleProjectedNearestPoint)

FPCGExSampleProjectedNearestPointContext::~FPCGExSampleProjectedNearestPointContext()
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
	PCGEX_DELETE(ProjectedTargetOctree)

	PCGEX_DELETE_TARRAY(BlendOps)
	PCGEX_DELETE(PropertiesBlender)

	ProjectionSettings.Cleanup();

	PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_DELETE)

	ProjectedSourceIO.Empty();
	ProjectedTargetIO.Empty();
}

bool FPCGExSampleProjectedNearestPointElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleProjectedNearestPoint)

	PCGEX_FWD(ProjectionSettings)

	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGEx::SourceTargetsLabel);
	if (!Targets.IsEmpty())
	{
		const FPCGTaggedData& Target = Targets[0];
		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Target.Data))
		{
			if (SpatialData->ToPointData(Context))
			{
				Context->Targets = PCGExData::PCGExPointIO::GetPointIO(Context, Target);
				// Prepare blend ops, if any

				PCGEx::FAttributesInfos* AttInfos = PCGEx::FAttributesInfos::Get(Context->Targets->GetIn()->Metadata);

				for (const TPair<FName, EPCGExDataBlendingType>& BlendPair : Settings->TargetAttributes)
				{
					const PCGEx::FAttributeIdentity* Identity = AttInfos->Find(BlendPair.Key);
					if (!Identity)
					{
						PCGE_LOG(Warning, GraphAndLog, FText::Format(FTEXT("Attribute '{0}' does not exists on target."), FText::FromName(BlendPair.Key)));
						continue;
					}

					Context->BlendOps.Add(PCGExDataBlending::CreateOperation(BlendPair.Value, *Identity));
				}

				delete AttInfos;
			}
		}
	}

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	Context->RangeMinGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->RangeMinGetter->Capture(Settings->LocalRangeMin);

	Context->RangeMaxGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->RangeMaxGetter->Capture(Settings->LocalRangeMax);

	PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_FWD)

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

	PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->LookAtUpGetter = new PCGEx::FLocalVectorGetter();
	Context->LookAtUpGetter->Capture(Settings->LookAtUpSource);

	if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
	{
		if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
		{
			if (!Context->LookAtUpGetter->Grab(*Context->Targets)) { PCGE_LOG(Warning, GraphAndLog, FTEXT("LookUp is invalid on target.")); }
		}
	}

	Context->Targets->CreateInKeys();

	PCGExDataFilter::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, {PCGExFactories::EType::Filter}, false);
	PCGExDataFilter::GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, Context->ValueFilterFactories, {PCGExFactories::EType::Filter}, false);

	if (Settings->bBlendPointProperties)
	{
		Context->PropertiesBlender = new PCGExDataBlending::FPropertiesBlender(Settings->PointPropertiesBlendingSettings);
	}

	return true;
}

bool FPCGExSampleProjectedNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleProjectedNearestPointElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleProjectedNearestPoint)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->Targets->CreateInKeys();
		Context->ProjectionSettings.Init(Context->Targets);
		Context->SetState(PCGExGeo::State_ProcessingProjectedPoints);
	}

	if (Context->IsState(PCGExGeo::State_ProcessingProjectedPoints))
	{
		auto Initialize = [&]()
		{
			Context->ProjectedTargetIO.SetNumUninitialized(Context->Targets->GetNum());
		};

		auto ProcessPoint = [&](const int32 ReadIndex)
		{
			FPCGPoint& Pt = Context->ProjectedTargetIO[ReadIndex] = Context->Targets->GetInPoint(ReadIndex);
			FVector Pos = Context->ProjectionSettings.Project(Pt.Transform.GetLocation());
			Pos.Z = 0;
			Pt.Transform.SetLocation(Pos);
		};

		if (!Context->Process(Initialize, ProcessPoint, Context->Targets->GetNum())) { return false; }

		FBox OctreeBounds = FBox(ForceInit);
		for (const FPCGPoint& Pt : Context->ProjectedTargetIO) { OctreeBounds += Pt.Transform.GetLocation(); }
		Context->ProjectedTargetOctree = new TOctree2<FPCGPointRef, FPCGPointRefSemantics>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
		for (const FPCGPoint& Pt : Context->ProjectedTargetIO) { Context->ProjectedTargetOctree->AddElement(FPCGPointRef(Pt)); }

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			Context->CurrentIO->CreateOutKeys();
			Context->ProjectionSettings.Init(Context->CurrentIO);

			for (PCGExDataBlending::FDataBlendingOperationBase* Op : Context->BlendOps)
			{
				Op->PrepareForData(*Context->CurrentIO, *Context->Targets);
			}

			if (!Context->PointFilterFactories.IsEmpty() || !Context->ValueFilterFactories.IsEmpty())
			{
				Context->SetState(PCGExDataFilter::State_FilteringPoints);
			}
			else
			{
				Context->SetState(PCGExGeo::State_PreprocessPositions);
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

		Context->SetState(PCGExGeo::State_PreprocessPositions);
	}

	if (Context->IsState(PCGExGeo::State_PreprocessPositions))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			if (Settings->bWriteLookAtTransform)
			{
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Source &&
					!Context->LookAtUpGetter->Grab(PointIO))
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("LookUp is invalid on source."));
				}
			}

			Context->ProjectedSourceIO.SetNumUninitialized(PointIO.GetNum());
		};

		auto ProcessPoint = [&](const int32 ReadIndex, const PCGExData::FPointIO& PointIO)
		{
			FPCGPoint& Pt = Context->ProjectedSourceIO[ReadIndex] = PointIO.GetInPoint(ReadIndex);
			FVector Pos = Context->ProjectionSettings.Project(Pt.Transform.GetLocation());
			Pos.Z = 0;
			Pt.Transform.SetLocation(Pos);
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

			PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_ACCESSOR_INIT)
		};

		auto ProcessPoint = [&](const int32 ReadIndex, const PCGExData::FPointIO& PointIO)
		{
			Context->GetAsyncManager()->Start<FPCGExSampleProjectedPointTask>(ReadIndex, const_cast<PCGExData::FPointIO*>(&PointIO));
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint)) { return false; }
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		for (PCGExDataBlending::FDataBlendingOperationBase* Op : Context->BlendOps) { Op->Write(); }

		PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_WRITE)
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

bool FPCGExSampleProjectedPointTask::ExecuteTask()
{
	const FPCGExSampleProjectedNearestPointContext* Context = Manager->GetContext<FPCGExSampleProjectedNearestPointContext>();
	PCGEX_SETTINGS(SampleProjectedNearestPoint)

	if (Context->PointFilterManager && !Context->PointFilterManager->Results[TaskIndex]) { return false; }

	const bool bSingleSample = (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget);

	const TArray<FPCGPoint>& TargetPoints = Context->Targets->GetIn()->GetPoints();
	const int32 NumTargets = Context->ProjectedTargetIO.Num();
	const FPCGPoint& SourcePoint = PointIO->GetInPoint(TaskIndex);
	const FPCGPoint& ProjectedSourcePoint = Context->ProjectedSourceIO[TaskIndex];
	const FVector ProjectedSourceCenter = ProjectedSourcePoint.Transform.GetLocation();

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

		Settings->DistanceSettings.GetCenters(ProjectedSourcePoint, Target, A, B);

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
		const FBox Box = FBoxCenterAndExtent(ProjectedSourceCenter, FVector(FMath::Sqrt(RangeMax))).GetBox();
		auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
		{
			const ptrdiff_t PointIndex = InPointRef.Point - Context->ProjectedTargetIO.GetData();
			if (!Context->ProjectedTargetIO.IsValidIndex(PointIndex)) { return; }

			ProcessTarget(PointIndex, Context->ProjectedTargetIO[PointIndex]);
		};

		Context->ProjectedTargetOctree->FindElementsWithBoundsTest(Box, ProcessNeighbor);
	}
	else
	{
		for (int i = 0; i < NumTargets; i++) { ProcessTarget(i, Context->ProjectedTargetIO[i]); }
	}

	// Compound never got updated, meaning we couldn't find target in range
	if (TargetsCompoundInfos.UpdateCount <= 0)
	{
		double FailSafeDist = FMath::Sqrt(RangeMax);
		PCGEX_OUTPUT_VALUE(Success, TaskIndex, false)
		PCGEX_OUTPUT_VALUE(Transform, TaskIndex, ProjectedSourcePoint.Transform)
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
	FVector WeightedProjectedPosition = FVector::Zero();
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

		WeightedProjectedPosition += Context->ProjectedTargetIO[TargetInfos.Index].Transform.GetLocation();

		if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += Context->LookAtUpGetter->SafeGet(TargetInfos.Index, Context->SafeUpVector) * Weight; }

		WeightedSignAxis += PCGExMath::GetDirection(Target.Transform.GetRotation(), Settings->SignAxis) * Weight;
		WeightedAngleAxis += PCGExMath::GetDirection(Target.Transform.GetRotation(), Settings->AngleAxis) * Weight;

		TotalWeight += Weight;

		for (const PCGExDataBlending::FDataBlendingOperationBase* Op : Context->BlendOps) { Op->DoOperation(TaskIndex, TargetInfos.Index, TaskIndex, Weight); }
	};

	for (PCGExDataBlending::FDataBlendingOperationBase* Op : Context->BlendOps) { if (Op->GetRequiresPreparation()) { Op->PrepareOperation(TaskIndex); } }

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

	for (PCGExDataBlending::FDataBlendingOperationBase* Op : Context->BlendOps) { if (Op->GetRequiresFinalization()) { Op->FinalizeOperation(TaskIndex, Count, TotalWeight); } }

	if (TotalWeight != 0) // Dodge NaN
	{
		WeightedProjectedPosition /= TotalWeight;
		WeightedUp /= TotalWeight;

		WeightedTransform.SetRotation(WeightedTransform.GetRotation() / TotalWeight);
		WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() / TotalWeight);
		WeightedTransform.SetLocation(WeightedTransform.GetLocation() / TotalWeight);
	}

	WeightedUp.Normalize();

	FVector LookAt = (ProjectedSourcePoint.Transform.GetLocation() - WeightedProjectedPosition).GetSafeNormal();
	const double WeightedDistance = FVector::Dist(ProjectedSourcePoint.Transform.GetLocation(), WeightedProjectedPosition);

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
