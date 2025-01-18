// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestBounds.h"


#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestBoundsElement"
#define PCGEX_NAMESPACE SampleNearestBounds

namespace PCGExNearestBounds
{
	void FSamplesStats::Update(const FSample& InSample)
	{
		UpdateCount++;

		if (InSample.DistanceSquared < SampledRangeMin)
		{
			Closest = InSample;
			SampledRangeMin = InSample.DistanceSquared;
		}
		else if (InSample.DistanceSquared > SampledRangeMax)
		{
			Farthest = InSample;
			SampledRangeMax = InSample.DistanceSquared;
		}

		if (InSample.SizeSquared > SampledLengthMax)
		{
			Largest = InSample;
			SampledLengthMax = InSample.SizeSquared;
		}
		else if (InSample.SizeSquared < SampledLengthMin)
		{
			Smallest = InSample;
			SampledLengthMin = InSample.SizeSquared;
		}
	}

	void FSamplesStats::Replace(const FSample& InSample)
	{
		UpdateCount++;

		Closest = InSample;
		SampledRangeMin = InSample.DistanceSquared;
		Farthest = InSample;
		SampledRangeMax = InSample.DistanceSquared;
		Largest = InSample;
		SampledLengthMax = InSample.SizeSquared;
		Smallest = InSample;
		SampledLengthMin = InSample.SizeSquared;
	}
}

UPCGExSampleNearestBoundsSettings::UPCGExSampleNearestBoundsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightRemap) { WeightRemap = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestBoundsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "The bounds data set to check against.", Required, {})
	if (SampleMethod == EPCGExBoundsSampleMethod::BestCandidate)
	{
		PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleNearestBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

void FPCGExSampleNearestBoundsContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleNearestBounds)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightRemap.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestBounds)

bool FPCGExSampleNearestBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)

	Context->BoundsFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceBoundsLabel, true);
	if (!Context->BoundsFacade) { return false; }

	TSet<FName> MissingTargetAttributes;
	PCGExDataBlending::AssembleBlendingDetails(
		Settings->bBlendPointProperties ? Settings->PointPropertiesBlendingSettings : FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None),
		Settings->TargetAttributes, Context->BoundsFacade->Source, Context->BlendingDetails, MissingTargetAttributes);

	for (const FName Id : MissingTargetAttributes) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on targets: {0}."), FText::FromName(Id))); }


	PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->BoundsPoints = &Context->BoundsFacade->Source->GetIn()->GetPoints();
	Context->BoundsPreloader = MakeShared<PCGExData::FFacadePreloader>();

	if (Settings->SampleMethod == EPCGExBoundsSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::PointSorter<false>>(Context, Context->BoundsFacade.ToSharedRef(), PCGExSorting::GetSortingRules(InContext, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
		Context->Sorter->RegisterBuffersDependencies(*Context->BoundsPreloader);
	}

	Context->BlendingDetails.RegisterBuffersDependencies(Context, Context->BoundsFacade, *Context->BoundsPreloader);

	return true;
}

void FPCGExSampleNearestBoundsElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)

	Context->RuntimeWeightCurve = Settings->LocalWeightRemap;
	if (!Settings->bUseLocalCurve)
	{
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(0, 0);
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(1, 1);
		Context->RuntimeWeightCurve.ExternalCurve = Settings->WeightRemap.Get();
	}
	Context->WeightCurve = Context->RuntimeWeightCurve.GetRichCurveConst();
}

bool FPCGExSampleNearestBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGEx::State_FacadePreloading);
		Context->PauseContext();
		Context->BoundsPreloader->OnCompleteCallback = [Settings, Context]()
		{
			if (Context->Sorter && !Context->Sorter->Init())
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestBounds::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleNearestBounds::FProcessor>>& NewBatch)
				{
					if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
				}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->BoundsPreloader->StartLoading(Context->GetAsyncManager(), Context->BoundsFacade.ToSharedRef());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleNearestBounds
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::SamplingFailed(const int32 Index, const FPCGPoint& Point)
	{
		SampleState[Index] = false;

		constexpr double FailSafeDist = -1;
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestBounds::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_INIT)
		}

		if (Context->BlendingDetails.HasAnyBlending())
		{
			Blender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Context->BlendingDetails);
			Blender->PrepareForData(PointDataFacade, Context->BoundsFacade.ToSharedRef());
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { LookAtUpGetter = Context->BoundsFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }
			else { LookAtUpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }

			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("LookAtUp is invalid.")); }
		}

		bSingleSample = Settings->SampleMethod != EPCGExBoundsSampleMethod::WithinRange;

		Cloud = Context->BoundsFacade->GetCloud(Settings->BoundsSource);
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TPointsProcessor<FPCGExSampleNearestBoundsContext, UPCGExSampleNearestBoundsSettings>::PrepareLoopScopesForPoints(Loops);
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

		TArray<PCGExNearestBounds::FSample> Samples;
		PCGExNearestBounds::FSamplesStats Stats;
		PCGExGeo::FSample CurrentSample;

		const FVector Origin = Point.Transform.GetLocation();

		const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Origin, PCGExMath::GetLocalBounds(Point, BoundsSource).GetExtent());
		Cloud->GetOctree()->FindElementsWithBoundsTest(
			BCAE, [&](const PCGExGeo::FPointBox* NearbyBox)
			{
				NearbyBox->Sample(Point, CurrentSample);
				CurrentSample.Weight = Context->WeightCurve->Eval(CurrentSample.Weight);

				if (!CurrentSample.bIsInside) { return; }

				if (bSingleSample)
				{
					if (Settings->SampleMethod == EPCGExBoundsSampleMethod::BestCandidate && Stats.IsValid())
					{
						if (!Context->Sorter->Sort(NearbyBox->Index, Stats.Closest.Index)) { return; }
						Stats.Replace(PCGExNearestBounds::FSample(CurrentSample, NearbyBox->RadiusSquared));
					}
					else
					{
						Stats.Update(PCGExNearestBounds::FSample(CurrentSample, NearbyBox->RadiusSquared));
					}
				}
				else
				{
					const PCGExNearestBounds::FSample& Infos = Samples.Emplace_GetRef(CurrentSample, NearbyBox->RadiusSquared);
					Stats.Update(Infos);
				}
			});

		// Compound never got updated, meaning we couldn't find target in range
		if (Stats.UpdateCount <= 0)
		{
			SamplingFailed(Index, Point);
			return;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);
		FVector WeightedUp = SafeUpVector;
		if (Settings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double TotalWeight = 0;
		double TotalSamples = 0;

		auto ProcessSample = [&]
			(const PCGExNearestBounds::FSample& InSample)
		{
			const double Weight = InSample.Weight;
			const FPCGPoint& Target = Context->BoundsFacade->Source->GetInPoint(InSample.Index);

			const FTransform TargetTransform = Target.Transform;
			const FQuat TargetRotation = TargetTransform.GetRotation();

			WeightedTransform = PCGExMath::WeightedAdd(WeightedTransform, TargetTransform, Weight);
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { PCGExMath::WeightedAdd(WeightedUp, (LookAtUpGetter ? LookAtUpGetter->Read(InSample.Index) : SafeUpVector), Weight); }


			WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * Weight;

			TotalWeight += Weight;
			TotalSamples++;

			if (Blender) { Blender->Blend(Index, InSample.Index, Index, Weight); }
		};

		if (Blender) { Blender->PrepareForBlending(Index, &Point); }

		if (bSingleSample)
		{
			switch (Settings->SampleMethod)
			{
			default: ;
			case EPCGExBoundsSampleMethod::WithinRange:
				// Ignore
				break;
			case EPCGExBoundsSampleMethod::BestCandidate:
			case EPCGExBoundsSampleMethod::ClosestBounds:
				ProcessSample(Stats.Closest);
				break;
			case EPCGExBoundsSampleMethod::FarthestBounds:
				ProcessSample(Stats.Farthest);
				break;
			case EPCGExBoundsSampleMethod::LargestBounds:
				ProcessSample(Stats.Largest);
				break;
			case EPCGExBoundsSampleMethod::SmallestBounds:
				ProcessSample(Stats.Smallest);
				break;
			}
		}
		else
		{
			for (PCGExNearestBounds::FSample& TargetInfos : Samples)
			{
				if (TargetInfos.Weight == 0) { continue; }
				ProcessSample(TargetInfos);
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
