// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleInsideBounds.h"


#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExSampleInsideBoundsElement"
#define PCGEX_NAMESPACE SampleInsideBounds

namespace PCGExInsideBounds
{
	void FSamplesStats::Update(const FSample& InSample)
	{
		UpdateCount++;

		if (InSample.Distance < SampledRangeMin)
		{
			Closest = InSample;
			SampledRangeMin = InSample.Distance;
		}

		if (InSample.Distance > SampledRangeMax)
		{
			Farthest = InSample;
			SampledRangeMax = InSample.Distance;
		}

		SampledRangeWidth = SampledRangeMax - SampledRangeMin;
	}

	void FSamplesStats::Replace(const FSample& InSample)
	{
		UpdateCount++;

		Closest = InSample;
		SampledRangeMin = InSample.Distance;
		Farthest = InSample;
		SampledRangeMax = InSample.Distance;

		SampledRangeWidth = SampledRangeMax - SampledRangeMin;
	}
}

UPCGExSampleInsideBoundsSettings::UPCGExSampleInsideBoundsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleInsideBoundsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "The point data set to check against.", Required, {})
	if (SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	PCGEX_PIN_FACTORIES(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleInsideBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

void FPCGExSampleInsideBoundsContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleInsideBounds)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightOverDistance.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleInsideBounds)

bool FPCGExSampleInsideBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleInsideBounds)

	Context->TargetsFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, true);
	if (!Context->TargetsFacade) { return false; }

	Context->TargetsPreloader = MakeShared<PCGExData::FFacadePreloader>();

	TSet<FName> MissingTargetAttributes;
	PCGExDataBlending::AssembleBlendingDetails(
		Settings->bBlendPointProperties ? Settings->PointPropertiesBlendingSettings : FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None),
		Settings->TargetAttributes, Context->TargetsFacade->Source, Context->BlendingDetails, MissingTargetAttributes);

	for (const FName Id : MissingTargetAttributes) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on targets: {0}."), FText::FromName(Id))); }


	PCGEX_FOREACH_FIELD_INSIDEBOUNDS(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->DistanceDetails = Settings->DistanceDetails.MakeDistances();

	Context->TargetPoints = &Context->TargetsFacade->Source->GetIn()->GetPoints();

	Context->NumTargets = Context->TargetPoints->Num();
	Context->TargetOctree = &Context->TargetsFacade->Source->GetIn()->GetOctree();

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::PointSorter<false>>(Context, Context->TargetsFacade.ToSharedRef(), PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
		Context->Sorter->RegisterBuffersDependencies(*Context->TargetsPreloader);
	}

	Context->BlendingDetails.RegisterBuffersDependencies(Context, Context->TargetsFacade, *Context->TargetsPreloader);

	return true;
}

void FPCGExSampleInsideBoundsElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(SampleInsideBounds)

	Context->RuntimeWeightCurve = Settings->LocalWeightOverDistance;

	if (!Settings->bUseLocalCurve)
	{
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(0, 0);
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(1, 1);
		Context->RuntimeWeightCurve.ExternalCurve = Settings->WeightOverDistance.Get();
	}

	Context->WeightCurve = Context->RuntimeWeightCurve.GetRichCurveConst();
}

bool FPCGExSampleInsideBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleInsideBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleInsideBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGEx::State_FacadePreloading);
		Context->TargetsPreloader->OnCompleteCallback = [Context, Settings]()
		{
			if (Context->Sorter && !Context->Sorter->Init())
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleInsideBoundss::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleInsideBoundss::FProcessor>>& NewBatch)
				{
					if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
				}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->TargetsPreloader->StartLoading(Context->GetAsyncManager(), Context->TargetsFacade.ToSharedRef());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleInsideBoundss
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::SamplingFailed(const int32 Index, const FPCGPoint& Point)
	{
		SampleState[Index] = false;

		const double FailSafeDist = RangeMaxGetter ? FMath::Sqrt(RangeMaxGetter->Read(Index)) : Settings->RangeMax;
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, FVector(FailSafeDist))
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
		PCGEX_OUTPUT_VALUE(SampledIndex, Index, -1)
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleInsideBoundss::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_INSIDEBOUNDS(PCGEX_OUTPUT_INIT)
		}

		if (!Context->BlendingDetails.FilteredAttributes.IsEmpty() ||
			!Context->BlendingDetails.GetPropertiesBlendingDetails().HasNoBlending())
		{
			Blender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Context->BlendingDetails);
			Blender->PrepareForData(PointDataFacade, Context->TargetsFacade.ToSharedRef());
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { LookAtUpGetter = Context->TargetsFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }
			else { LookAtUpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }

			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("LookAtUp is invalid.")); }
		}

		if (Settings->bUseLocalRangeMin)
		{
			RangeMinGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMin);
			if (!RangeMinGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("RangeMin metadata missing")); }
		}
		if (Settings->bUseLocalRangeMax)
		{
			RangeMaxGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMax);
			if (!RangeMaxGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("RangeMax metadata missing")); }
		}

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bSampleClosest = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::BestCandidate;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TPointsProcessor<FPCGExSampleInsideBoundsContext, UPCGExSampleInsideBoundsSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedValue<double>>(Loops, 0);
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index, Point); }
			return;
		}

		const FVector Origin = Point.Transform.GetLocation();

		double RangeMin = FMath::Square(RangeMinGetter ? RangeMinGetter->Read(Index) : Settings->RangeMin);
		double RangeMax = FMath::Square(RangeMaxGetter ? RangeMaxGetter->Read(Index) : Settings->RangeMax);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExInsideBounds::FSample> TargetsInfos;
		//TargetsInfos.Reserve(Context->Targets->GetNum());


		PCGExInsideBounds::FSamplesStats Stats;
		auto SampleTarget = [&](const int32 PointIndex, const FPCGPoint& Target)
		{
			//if (Context->ValueFilterManager && !Context->ValueFilterManager->Results[PointIndex]) { return; } // TODO : Implement

			FVector A;
			FVector B;

			Context->DistanceDetails->GetCenters(Point, Target, A, B);

			const double Dist = FVector::DistSquared(A, B);

			if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

			if (bSingleSample)
			{
				if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate && Stats.IsValid())
				{
					if (!Context->Sorter->Sort(PointIndex, Stats.Closest.Index)) { return; }
					Stats.Replace(PCGExInsideBounds::FSample(PointIndex, Dist));
				}
				else
				{
					Stats.Update(PCGExInsideBounds::FSample(PointIndex, Dist));
				}
			}
			else
			{
				const PCGExInsideBounds::FSample& Infos = TargetsInfos.Emplace_GetRef(PointIndex, Dist);
				Stats.Update(Infos);
			}
		};

		if (RangeMax > 0)
		{
			const FBox Box = FBoxCenterAndExtent(Origin, FVector(FMath::Sqrt(RangeMax))).GetBox();
			auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t PointIndex = InPointRef.Point - Context->TargetPoints->GetData();
				SampleTarget(PointIndex, *(Context->TargetPoints->GetData() + PointIndex));
			};

			Context->TargetOctree->FindElementsWithBoundsTest(Box, ProcessNeighbor);
		}
		else
		{
			TargetsInfos.Reserve(Context->NumTargets);
			for (int i = 0; i < Context->NumTargets; i++) { SampleTarget(i, *(Context->TargetPoints->GetData() + i)); }
		}

		// Compound never got updated, meaning we couldn't find target in range
		if (Stats.UpdateCount <= 0)
		{
			SamplingFailed(Index, Point);
			return;
		}

		// Compute individual target weight
		if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0)
		{
			// Reset compounded infos to full range
			Stats.SampledRangeMin = RangeMin;
			Stats.SampledRangeMax = RangeMax;
			Stats.SampledRangeWidth = RangeMax - RangeMin;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);
		FVector WeightedUp = SafeUpVector;
		if (Settings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double TotalWeight = 0;
		double TotalSamples = 0;

		auto ProcessTargetInfos = [&]
			(const PCGExInsideBounds::FSample& TargetInfos, const double Weight)
		{
			const FPCGPoint& Target = Context->TargetsFacade->Source->GetInPoint(TargetInfos.Index);

			const FTransform TargetTransform = Target.Transform;
			const FQuat TargetRotation = TargetTransform.GetRotation();

			WeightedTransform = PCGExMath::WeightedAdd(WeightedTransform, TargetTransform, Weight);
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { PCGExMath::WeightedAdd(WeightedUp, (LookAtUpGetter ? LookAtUpGetter->Read(TargetInfos.Index) : SafeUpVector), Weight); }

			WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * Weight;

			TotalWeight += Weight;
			TotalSamples++;

			if (Blender) { Blender->Blend(Index, TargetInfos.Index, Index, Weight); }
		};

		if (Blender) { Blender->PrepareForBlending(Index, &Point); }

		if (bSingleSample)
		{
			const PCGExInsideBounds::FSample& TargetInfos = bSampleClosest ? Stats.Closest : Stats.Farthest;
			const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExInsideBounds::FSample& TargetInfos : TargetsInfos)
			{
				const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
				if (Weight == 0) { continue; }
				ProcessTargetInfos(TargetInfos, Weight);
			}
		}

		if (Blender) { Blender->CompleteBlending(Index, TotalSamples, TotalWeight); }

		if (TotalWeight != 0) // Dodge NaN
		{
			WeightedUp /= TotalWeight;
			WeightedTransform = PCGExMath::Div(WeightedTransform, TotalWeight);
		}

		WeightedUp.Normalize();

		const FVector CWDistance = Origin - WeightedTransform.GetLocation();
		FVector LookAt = CWDistance.GetSafeNormal();
		const double WeightedDistance = FVector::Dist(Origin, WeightedTransform.GetLocation());

		SampleState[Index] = Stats.IsValid();
		PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(NumSamples, Index, TotalSamples)
		PCGEX_OUTPUT_VALUE(SampledIndex, Index, Stats.IsValid() ? Stats.Closest.Index : -1)

		MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), WeightedDistance));

		FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		PCGExSampling::PruneFailedSamples(PointDataFacade->GetMutablePoints(), SampleState);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
