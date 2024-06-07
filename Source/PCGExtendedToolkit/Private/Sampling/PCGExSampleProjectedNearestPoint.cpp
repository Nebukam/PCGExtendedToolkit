// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleProjectedNearestPoint.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataFilter.h"
#include "Data/Blending/PCGExMetadataBlender.h"
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

	PCGEX_DELETE(Blender)

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

	PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_FWD)

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

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourcePointFilters, Context->PointFilterFactories, {PCGExFactories::EType::Filter}, false);
	PCGExFactories::GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, Context->ValueFilterFactories, {PCGExFactories::EType::Filter}, false);

	return true;
}

bool FPCGExSampleProjectedNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
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
		PCGEX_DELETE(Context->PointFilterManager)
		PCGEX_DELETE(Context->ValueFilterManager)

		if (!Context->AdvancePointsIO())
		{
			Context->Done();
			Context->ExecutionComplete();
		}
		else
		{
			Context->CurrentIO->CreateOutKeys();
			Context->ProjectionSettings.Init(Context->CurrentIO);

			if (Context->Blender) { Context->Blender->PrepareForData(*Context->CurrentIO, *Context->Targets); }

			bool bHasHeavyPointFilters = false;
			bool bHasHeavyValueFilters = false;

			if (!Context->PointFilterFactories.IsEmpty())
			{
				Context->PointFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(Context->CurrentIO);
				Context->PointFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->PointFilterFactories, Context->CurrentIO);
				bHasHeavyPointFilters = Context->PointFilterManager->PrepareForTesting();
			}

			if (!Context->ValueFilterFactories.IsEmpty())
			{
				Context->ValueFilterManager = new PCGExDataFilter::TEarlyExitFilterManager(Context->CurrentIO);
				Context->ValueFilterManager->Register<UPCGExFilterFactoryBase>(Context, Context->ValueFilterFactories, Context->CurrentIO);
				bHasHeavyValueFilters = Context->ValueFilterManager->PrepareForTesting();
			}

			if (Context->PointFilterManager || Context->ValueFilterManager)
			{
				if (bHasHeavyPointFilters || bHasHeavyValueFilters) { Context->SetState(PCGExDataFilter::State_PreparingFilters); }
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
		auto PreparePoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			if (Context->PointFilterManager) { Context->PointFilterManager->PrepareSingle(Index); }
			if (Context->ValueFilterManager) { Context->ValueFilterManager->PrepareSingle(Index); }
		};

		if (!Context->ProcessCurrentPoints(PreparePoint)) { return false; }

		if (Context->PointFilterManager) { Context->PointFilterManager->PreparationComplete(); }
		if (Context->ValueFilterManager) { Context->ValueFilterManager->PreparationComplete(); }

		Context->SetState(PCGExDataFilter::State_FilteringPoints);
	}

	if (Context->IsState(PCGExDataFilter::State_FilteringPoints))
	{
		auto ProcessPoint = [&](const int32 PointIndex, const PCGExData::FPointIO& PointIO)
		{
			if (Context->PointFilterManager) { Context->PointFilterManager->Test(PointIndex); }
			if (Context->ValueFilterManager) { Context->ValueFilterManager->Test(PointIndex); }
		};

		if (!Context->ProcessCurrentPoints(ProcessPoint)) { return false; }

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
		Context->StartAsyncLoopEx<PCGExSampleNearestProjectedPointTasks::FPointLoop>(Context->CurrentIO, Context->CurrentIO->GetNum());
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		if (Context->Blender) { Context->Blender->Write(); }

		PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_WRITE)
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

namespace PCGExSampleNearestProjectedPointTasks
{
	bool FPointLoop::LoopInit()
	{
		const FPCGExSampleProjectedNearestPointContext* Context = Manager->GetContext<FPCGExSampleProjectedNearestPointContext>();
		PCGEX_SETTINGS(SampleProjectedNearestPoint)

		if (Settings->bUseLocalRangeMin)
		{
			if (Context->RangeMinGetter->Grab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMin metadata missing")); }
		}

		if (Settings->bUseLocalRangeMax)
		{
			if (Context->RangeMaxGetter->Grab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMax metadata missing")); }
		}

		PCGEX_FOREACH_FIELD_PROJECTNEARESTPOINT(PCGEX_OUTPUT_ACCESSOR_INIT_PTR)

		return true;
	}

	void FSamplePoint::LoopBody(const int32 Iteration)
	{
		const FPCGExSampleProjectedNearestPointContext* Context = Manager->GetContext<FPCGExSampleProjectedNearestPointContext>();
		PCGEX_SETTINGS(SampleProjectedNearestPoint)

		if (Context->PointFilterManager && !Context->PointFilterManager->Results[Iteration]) { return; }

		const bool bSingleSample = (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget);

		const TArray<FPCGPoint>& TargetPoints = Context->Targets->GetIn()->GetPoints();
		const int32 NumTargets = Context->ProjectedTargetIO.Num();
		const FPCGPoint& SourcePoint = PointIO->GetInPoint(Iteration);
		const FPCGPoint& ProjectedSourcePoint = Context->ProjectedSourceIO[Iteration];
		const FVector ProjectedSourceCenter = ProjectedSourcePoint.Transform.GetLocation();

		double RangeMin = FMath::Pow(Context->RangeMinGetter->SafeGet(Iteration, Settings->RangeMin), 2);
		double RangeMax = FMath::Pow(Context->RangeMaxGetter->SafeGet(Iteration, Settings->RangeMax), 2);

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
			PCGEX_OUTPUT_VALUE(Success, Iteration, false)
			PCGEX_OUTPUT_VALUE(Transform, Iteration, ProjectedSourcePoint.Transform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Iteration, SourcePoint.Transform)
			PCGEX_OUTPUT_VALUE(Distance, Iteration, FailSafeDist)
			PCGEX_OUTPUT_VALUE(SignedDistance, Iteration, FailSafeDist)
			PCGEX_OUTPUT_VALUE(NumSamples, Iteration, 0)
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
			                     Context->LookAtUpGetter->SafeGet(Iteration, Context->SafeUpVector) :
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

			if (Context->Blender) { Context->Blender->Blend(Iteration, TargetInfos.Index, Iteration, Weight); }
		};

		if (Context->Blender) { Context->Blender->PrepareForBlending(Iteration, &SourcePoint); }

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

		if (Context->Blender) { Context->Blender->CompleteBlending(Iteration, Count, TotalWeight); }

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

		PCGEX_OUTPUT_VALUE(Success, Iteration, TargetsCompoundInfos.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Iteration, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Iteration, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Iteration, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Iteration, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
		PCGEX_OUTPUT_VALUE(Angle, Iteration, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(NumSamples, Iteration, Count)
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
