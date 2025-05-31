// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestBounds.h"


#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Geometry/PCGExGeoPointBox.h"


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

	if (BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Normal, {})
	}
	else
	{
		PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations. These are currently ignored, but will preserve pin connections", Advanced, {})
	}

	if (SampleMethod == EPCGExBoundsSampleMethod::BestCandidate)
	{
		PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}

	return PinProperties;
}

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

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	Context->BoundsFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceBoundsLabel, false, true);
	if (!Context->BoundsFacade) { return false; }

	PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
			Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
			{PCGExFactories::EType::Blending}, false);
	}

	Context->BoundsPreloader = MakeShared<PCGExData::FFacadePreloader>(Context->BoundsFacade);

	if (Settings->SampleMethod == EPCGExBoundsSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::TPointSorter<>>(Context, Context->BoundsFacade.ToSharedRef(), PCGExSorting::GetSortingRules(InContext, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
		Context->Sorter->RegisterBuffersDependencies(*Context->BoundsPreloader);
	}

	PCGExDataBlending::RegisterBuffersDependencies_SourceA(Context, *Context->BoundsPreloader, Context->BlendingFactories);

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
					NewBatch->bRequiresWriteStep = Settings->bPruneFailedSamples;
				}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->BoundsPreloader->StartLoading(Context->GetAsyncManager());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExSampleNearestBoundsElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}

namespace PCGExSampleNearestBounds
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::SamplingFailed(const int32 Index)
	{
		SamplingMask[Index] = false;

		const TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		constexpr double FailSafeDist = -1;
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, Transforms[Index])
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Transforms[Index])
		PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? FailSafeDist : FailSafeDist * Settings->DistanceScale)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist * Settings->SignedDistanceScale)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, FVector(FailSafeDist))
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
		PCGEX_OUTPUT_VALUE(SampledIndex, Index, -1)
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestBounds::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;

		if (Context->ApplySampling.WantsApply())
		{
			AllocateFor |= EPCGPointNativeProperties::Transform;
		}

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_INIT)
		}

		if (!Context->BlendingFactories.IsEmpty())
		{
			BlendOpsManager = MakeShared<PCGExDataBlending::FBlendOpsManager>(PointDataFacade);
			BlendOpsManager->SetSourceA(Context->BoundsFacade); // We want operands A & B to be the vtx here

			if (!BlendOpsManager->Init(Context, Context->BlendingFactories)) { return false; }

			DataBlender = BlendOpsManager;
		}
		else if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic)
		{
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
			MetadataBlender->SetTargetData(PointDataFacade);
			MetadataBlender->SetSourceData(Context->BoundsFacade);

			TSet<FName> MissingAttributes;
			PCGExDataBlending::AssembleBlendingDetails(
				Settings->PointPropertiesBlendingSettings, Settings->TargetAttributes,
				Context->BoundsFacade->Source, BlendingDetails, MissingAttributes);

			if (!MetadataBlender->Init(Context, BlendingDetails))
			{
				// Fail
				Context->CancelExecution(FString("Error initializing blending"));
				return false;
			}

			DataBlender = MetadataBlender;
		}

		if (!DataBlender) { DataBlender = MakeShared<PCGExDataBlending::FDummyBlender>(); }

		if (Settings->bWriteLookAtTransform)
		{
			LookAtUpGetter = Settings->GetValueSettingLookAtUp();
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { if (!LookAtUpGetter->Init(Context, Context->BoundsFacade, false)) { return false; } }
			else { if (!LookAtUpGetter->Init(Context, PointDataFacade)) { return false; } }
		}
		else
		{
			LookAtUpGetter = PCGExDetails::MakeSettingValue(Settings->LookAtUpConstant);
		}

		bSingleSample = Settings->SampleMethod != EPCGExBoundsSampleMethod::WithinRange;

		Cloud = Context->BoundsFacade->GetCloud(Settings->BoundsSource);
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TPointsProcessor<FPCGExSampleNearestBoundsContext, UPCGExSampleNearestBoundsSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestBounds::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bLocalAnySuccess = false;

		TArray<PCGEx::FOpStats> BlendTrackers;

		if (DataBlender) { DataBlender->InitTrackers(BlendTrackers); }

		TArray<PCGExNearestBounds::FSample> Samples;
		Samples.Reserve(10);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();
		TConstPCGValueRange<FTransform> BoundsTransforms = Context->BoundsFacade->GetIn()->GetConstTransformValueRange();


		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			Samples.Reset();
			PCGExNearestBounds::FSamplesStats Stats;
			PCGExGeo::FSample CurrentSample;

			const PCGExData::FMutablePoint Point = PointDataFacade->GetOutPoint(Index);
			const FVector Origin = Transforms[Index].GetLocation();

			const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Origin, PCGExMath::GetLocalBounds(Point, BoundsSource).GetExtent());
			Cloud->GetOctree()->FindElementsWithBoundsTest(
				BCAE, [&](const PCGExGeo::FPointBox* NearbyBox)
				{
					NearbyBox->Sample(Origin, CurrentSample);
					if (!CurrentSample.bIsInside) { return; }

					CurrentSample.Weight = Context->WeightCurve->Eval(CurrentSample.Weight);


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
				SamplingFailed(Index);
				continue;
			}

			FTransform WeightedTransform = FTransform::Identity;
			WeightedTransform.SetScale3D(FVector::ZeroVector);
			FVector WeightedUp = SafeUpVector;
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

			FVector WeightedSignAxis = FVector::Zero();
			FVector WeightedAngleAxis = FVector::Zero();
			PCGEx::FOpStats SampleTracker{};

			auto ProcessSample = [&]
				(const PCGExNearestBounds::FSample& InSample,
				 const TSharedPtr<PCGExDataBlending::IBlender>& Blender = nullptr)
			{
				const double Weight = InSample.Weight;

				const FTransform& TargetTransform = BoundsTransforms[InSample.Index];
				const FQuat TargetRotation = TargetTransform.GetRotation();

				WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, TargetTransform, Weight);
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { PCGExBlend::WeightedAdd(WeightedUp, (LookAtUpGetter ? LookAtUpGetter->Read(InSample.Index) : SafeUpVector), Weight); }

				WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * Weight;
				WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * Weight;

				SampleTracker.Count++;
				SampleTracker.Weight += Weight;

				if (Blender) { Blender->MultiBlend(InSample.Index, Index, Weight, BlendTrackers); }
			};

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
					DataBlender->Blend(Stats.Closest.Index, Index, Stats.TotalWeight);
					break;
				case EPCGExBoundsSampleMethod::FarthestBounds:
					ProcessSample(Stats.Farthest);
					DataBlender->Blend(Stats.Farthest.Index, Index, Stats.TotalWeight);
					break;
				case EPCGExBoundsSampleMethod::LargestBounds:
					ProcessSample(Stats.Largest);
					DataBlender->Blend(Stats.Largest.Index, Index, Stats.TotalWeight);
					break;
				case EPCGExBoundsSampleMethod::SmallestBounds:
					ProcessSample(Stats.Smallest);
					DataBlender->Blend(Stats.Smallest.Index, Index, Stats.TotalWeight);
					break;
				}
			}
			else
			{
				DataBlender->BeginMultiBlend(Index, BlendTrackers);

				for (PCGExNearestBounds::FSample& TargetInfos : Samples)
				{
					if (TargetInfos.Weight == 0) { continue; }
					ProcessSample(TargetInfos, DataBlender);
				}

				DataBlender->EndMultiBlend(Index, BlendTrackers);
			}

			if (SampleTracker.Weight != 0) // Dodge NaN
			{
				WeightedUp /= SampleTracker.Weight;
				WeightedTransform = PCGExBlend::Div(WeightedTransform, SampleTracker.Weight);
			}

			WeightedUp.Normalize();

			const FVector CWDistance = Origin - WeightedTransform.GetLocation();
			FVector LookAt = CWDistance.GetSafeNormal();
			const double WeightedDistance = FVector::Dist(Origin, WeightedTransform.GetLocation());

			FTransform LookAtTransform = PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign);
			if (Context->ApplySampling.WantsApply())
			{
				PCGExData::FMutablePoint MutablePoint(OutPointData, Index);
				Context->ApplySampling.Apply(MutablePoint, WeightedTransform, LookAtTransform);
			}

			SamplingMask[Index] = Stats.IsValid();
			PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
			PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, LookAtTransform)
			PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? WeightedDistance : WeightedDistance * Settings->DistanceScale)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance * Settings->SignedDistanceScale)
			PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
			PCGEX_OUTPUT_VALUE(NumSamples, Index, SampleTracker.Count)
			PCGEX_OUTPUT_VALUE(SampledIndex, Index, Stats.IsValid() ? Stats.Closest.Index : -1)

			MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), WeightedDistance));
			bLocalAnySuccess = true;
		}

		if (bLocalAnySuccess) { FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!Settings->bOutputNormalizedDistance || !DistanceWriter) { return; }

		MaxDistance = MaxDistanceValue->Max();

		const int32 NumPoints = PointDataFacade->GetNum();

		if (Settings->bOutputOneMinusDistance)
		{
			for (int i = 0; i < NumPoints; i++)
			{
				double& D = DistanceWriter->GetMutable(i);
				D = (1 - (D / MaxDistance)) * Settings->DistanceScale;
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				double& D = DistanceWriter->GetMutable(i);
				D = (D / MaxDistance) * Settings->DistanceScale;
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		(void)PointDataFacade->Source->Gather(SamplingMask);
	}

	void FProcessor::Cleanup()
	{
		TPointsProcessor<FPCGExSampleNearestBoundsContext, UPCGExSampleNearestBoundsSettings>::Cleanup();
		BlendOpsManager.Reset();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
