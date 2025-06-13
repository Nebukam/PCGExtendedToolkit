// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExBlendOpFactoryProvider.h"
#include "Data/Blending/PCGExBlendOpsManager.h"
#include "Data/Blending/PCGExMetadataBlender.h"


#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"
#define PCGEX_NAMESPACE SampleNearestPoint

namespace PCGExNearestPoint
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

	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations, used by Individual (non-monolithic) blending interface.", Normal, {})

	if (SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "The point data set to check against.", Required, {})
		PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	else
	{
		PCGEX_PIN_POINTS(PCGEx::SourceTargetsLabel, "Point data sets to check against.", Required, {})
	}

	PCGEX_PIN_FACTORIES(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced, {})

	return PinProperties;
}

bool UPCGExSampleNearestPointSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExDataBlending::SourceBlendingLabel) { return BlendingInterface == EPCGExBlendingInterface::Individual; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

void FPCGExSampleNearestPointContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleNearestPoint)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightOverDistance.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestPoint)

bool FPCGExSampleNearestPointElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	if (Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
			Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
			{PCGExFactories::EType::Blending}, false);
	}

	FBox OctreeBounds = FBox(ForceInit);

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		// Only grab the first target
		if (TSharedPtr<PCGExData::FFacade> SingleFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, true, true))
		{
			SingleFacade->Idx = 0;

			Context->TargetFacades.Add(SingleFacade.ToSharedRef());
			Context->TargetOctrees.Add(&SingleFacade->GetIn()->GetPointOctree());

			OctreeBounds += SingleFacade->GetIn()->GetBounds();
		}
	}
	else
	{
		TSharedPtr<PCGExData::FPointIOCollection> Targets = MakeShared<PCGExData::FPointIOCollection>(
			Context, PCGEx::SourceTargetsLabel, PCGExData::EIOInit::NoInit, true);

		if (Targets->IsEmpty())
		{
			if (!Settings->bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (empty datasets)")); }
			return false;
		}

		Context->TargetFacades.Reserve(Targets->Pairs.Num());
		Context->TargetOctrees.Reserve(Targets->Pairs.Num());

		for (const TSharedPtr<PCGExData::FPointIO>& IO : Targets->Pairs)
		{
			TSharedPtr<PCGExData::FFacade> TargetFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
			TargetFacade->Idx = Context->TargetFacades.Num() - 1;

			Context->TargetFacades.Add(TargetFacade.ToSharedRef());
			Context->TargetOctrees.Add(&TargetFacade->GetIn()->GetPointOctree());

			OctreeBounds += TargetFacade->GetIn()->GetBounds();
		}
	}

	Context->TargetsOctree = MakeShared<PCGEx::FIndexedItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
	for (int i = 0; i < Context->TargetFacades.Num(); ++i) { Context->TargetsOctree->AddElement(PCGEx::FIndexedItem(i, Context->TargetFacades[i]->GetIn()->GetBounds())); }

	Context->TargetsPreloader = MakeShared<PCGExData::FMultiFacadePreloader>(Context->TargetFacades);

	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->DistanceDetails = Settings->DistanceDetails.MakeDistances();

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::TPointSorter<>>(Context, Context->TargetFacades[0], PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
		Context->TargetsPreloader->ForEach([&](PCGExData::FFacadePreloader& Preloader) { Context->Sorter->RegisterBuffersDependencies(Preloader); });
	}

	Context->TargetsPreloader->ForEach(
		[&](PCGExData::FFacadePreloader& Preloader)
		{
			if (Settings->WeightMode != EPCGExSampleWeightMode::Distance) { Preloader.Register<double>(Context, Settings->WeightAttribute); }
			PCGExDataBlending::RegisterBuffersDependencies_SourceA(Context, Preloader, Context->BlendingFactories);
		});

	return true;
}

void FPCGExSampleNearestPointElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	Context->RuntimeWeightCurve = Settings->LocalWeightOverDistance;

	if (!Settings->bUseLocalCurve)
	{
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(0, 0);
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(1, 1);
		Context->RuntimeWeightCurve.ExternalCurve = Settings->WeightOverDistance.Get();
	}

	Context->WeightCurve = Context->RuntimeWeightCurve.GetRichCurveConst();
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGEx::State_FacadePreloading);
		Context->TargetsPreloader->OnCompleteCallback = [Settings, Context]()
		{
			// Prep weights
			if (Settings->WeightMode != EPCGExSampleWeightMode::Distance)
			{
				for (const TSharedRef<PCGExData::FFacade>& Facade : Context->TargetFacades)
				{
					TSharedPtr<PCGExData::TBuffer<double>> Weight = Facade->GetBroadcaster<double>(Settings->WeightAttribute);
					if (!Weight)
					{
						Context->CancelExecution(TEXT("Weight Attribute on Targets is invalid."));
						return;
					}

					Context->TargetWeights.Add(Weight);
				}
			}

			// Prep look up getters
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
			{
				for (const TSharedRef<PCGExData::FFacade>& Facade : Context->TargetFacades)
				{
					TSharedPtr<PCGExDetails::TSettingValue<FVector>> LookAtUpGetter = Settings->GetValueSettingLookAtUp();
					if (!LookAtUpGetter->Init(Context, Facade, false))
					{
						Context->CancelExecution(TEXT("LookUp Attribute on Targets is invalid."));
						return;
					}

					Context->TargetLookAtUpGetters.Add(LookAtUpGetter);
				}
			}

			if (Context->Sorter && !Context->Sorter->Init())
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestPoints::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleNearestPoints::FProcessor>>& NewBatch)
				{
					NewBatch->bRequiresWriteStep = Settings->bPruneFailedSamples;
				}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->TargetsPreloader->StartLoading(Context->GetAsyncManager());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExSampleNearestPointElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}

namespace PCGExSampleNearestPoints
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::SamplingFailed(const int32 Index)
	{
		SamplingMask[Index] = false;

		const TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		const double FailSafeDist = RangeMaxGetter->Read(Index);
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestPoints::Process);

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
			PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_INIT)
		}

		if (!Context->BlendingFactories.IsEmpty())
		{
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, Context->DistanceDetails);
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetFacades)) { return false; }
			DataBlender = UnionBlendOpsManager;
		}
		else if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic)
		{
			TSet<FName> MissingAttributes;
			PCGExDataBlending::AssembleBlendingDetails(
				Settings->PointPropertiesBlendingSettings, Settings->TargetAttributes,
				PointDataFacade->Source, BlendingDetails, MissingAttributes);

			UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&BlendingDetails, nullptr, Context->DistanceDetails);
			UnionBlender->AddSources(Context->TargetFacades);
			if (!UnionBlender->Init(Context, PointDataFacade)) { return false; }
			DataBlender = UnionBlender;
		}
		else
		{
			DataBlender = MakeShared<PCGExDataBlending::FDummyUnionBlender>();
		}

		if (!DataBlender) { DataBlender = MakeShared<PCGExDataBlending::FDummyUnionBlender>(); }

		if (Settings->bWriteLookAtTransform)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
			{
			}
			else
			{
				LookAtUpGetter = Settings->GetValueSettingLookAtUp();
				if (!LookAtUpGetter->Init(Context, PointDataFacade)) { return false; }
			}
		}
		else
		{
			LookAtUpGetter = PCGExDetails::MakeSettingValue(Settings->LookAtUpConstant);
		}

		RangeMinGetter = Settings->GetValueSettingRangeMin();
		if (!RangeMinGetter->Init(Context, PointDataFacade)) { return false; }

		RangeMaxGetter = Settings->GetValueSettingRangeMax();
		if (!RangeMaxGetter->Init(Context, PointDataFacade)) { return false; }

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bSampleClosest = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::BestCandidate;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TPointsProcessor<FPCGExSampleNearestPointContext, UPCGExSampleNearestPointSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestPoint::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bLocalAnySuccess = false;

		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		DataBlender->InitTrackers(Trackers);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		TArray<PCGExNearestPoint::FSample> Samples;
		Samples.Reserve(Context->NumTargets / 2); // Yo that might be excessive

		const TSharedPtr<PCGExData::FUnionDataWeighted> Union = MakeShared<PCGExData::FUnionDataWeighted>();

		PCGEX_SCOPE_LOOP(Index)
		{
			Union->Reset();

			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			const PCGExData::FMutablePoint Point = PointDataFacade->GetOutPoint(Index);
			const FVector Origin = Transforms[Index].GetLocation();

			double RangeMin = FMath::Square(RangeMinGetter->Read(Index));
			double RangeMax = FMath::Square(RangeMaxGetter->Read(Index));

			if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

			Samples.Reset();
			PCGExNearestPoint::FSamplesStats Stats;

			auto SampleTarget = [&](const int32 Idx, const int32 TargetIndex)
			{
				//if (Context->ValueFilterManager && !Context->ValueFilterManager->Results[PointIndex]) { return; } // TODO : Implement

				const PCGExData::FConstPoint Target = Context->TargetFacades[Idx]->Source->GetInPoint(TargetIndex);

				double Dist = 0;

				if (Settings->DistanceDetails.bOverlapIsZero)
				{
					bool bOverlap = false;
					Dist = Context->DistanceDetails->GetDistSquared(Point, Target, bOverlap);
					if (bOverlap) { Dist = 0; }
				}
				else
				{
					Dist = Context->DistanceDetails->GetDistSquared(Point, Target);
				}

				if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

				if (Settings->WeightMode == EPCGExSampleWeightMode::Attribute) { Dist = Context->TargetWeights[Idx]->Read(TargetIndex); }
				else if (Settings->WeightMode == EPCGExSampleWeightMode::AttributeMult) { Dist *= Context->TargetWeights[Idx]->Read(TargetIndex); }

				if (bSingleSample)
				{
					if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate && Stats.IsValid())
					{
						if (!Context->Sorter->Sort(TargetIndex, Stats.Closest.Index)) { return; }
						Stats.Replace(PCGExNearestPoint::FSample(Idx, TargetIndex, Dist));
					}
					else
					{
						Stats.Update(PCGExNearestPoint::FSample(Idx, TargetIndex, Dist));
					}
				}
				else
				{
					const PCGExNearestPoint::FSample& Infos = Samples.Emplace_GetRef(Idx, TargetIndex, Dist);
					Stats.Update(Infos);
				}
			};

			if (RangeMax > 0)
			{
				const FBox Box = FBoxCenterAndExtent(Origin, FVector(FMath::Sqrt(RangeMax))).GetBox();
				Context->TargetsOctree->FindElementsWithBoundsTest(
					Box, [&](const PCGEx::FIndexedItem& Item)
					{
						Context->TargetOctrees[Item.Index]->FindElementsWithBoundsTest(
							Box, [&](const PCGPointOctree::FPointRef& PointRef)
							{
								SampleTarget(Item.Index, PointRef.Index);
							});
					});
			}
			else
			{
				for (int i = 0; i < Context->TargetFacades.Num(); i++)
				{
					for (int j = 0; j < Context->TargetFacades.Num(); j++)
					{
						SampleTarget(i, j);
					}
				}
			}

			// Compound never got updated, meaning we couldn't find target in range
			if (Stats.UpdateCount <= 0)
			{
				SamplingFailed(Index);
				continue;
			}

			// Compute individual target weight
			if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0)
			{
				// Reset compounded infos to full range
				Stats.SampledRangeMin = RangeMin;
				Stats.SampledRangeMax = FMath::Max(Stats.SampledRangeMax, RangeMax);
				Stats.SampledRangeWidth = RangeMax - RangeMin;
			}

			FTransform WeightedTransform = FTransform::Identity;
			WeightedTransform.SetScale3D(FVector::ZeroVector);
			FVector WeightedUp = SafeUpVector;
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Source) { WeightedUp = LookAtUpGetter->Read(Index); }

			FVector WeightedSignAxis = FVector::Zero();
			FVector WeightedAngleAxis = FVector::Zero();
			PCGEx::FOpStats SampleTracker{};
			double WeightedDistance = 0;

			auto ProcessTargetInfos = [&]
				(const PCGExNearestPoint::FSample& TargetInfos, const double Weight,
				 const TSharedPtr<PCGExDataBlending::IUnionBlender>& Blender = nullptr)
			{
				const FTransform& TargetTransform = Context->TargetFacades[TargetInfos.IOIndex]->GetIn()->GetTransform(TargetInfos.Index);
				const FQuat TargetRotation = TargetTransform.GetRotation();

				WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, TargetTransform, Weight);
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
				{
					PCGExBlend::WeightedAdd(WeightedUp, Context->TargetLookAtUpGetters[TargetInfos.IOIndex]->Read(TargetInfos.Index), Weight);
				}

				WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * Weight;
				WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * Weight;
				WeightedDistance += FMath::Sqrt(TargetInfos.Distance);

				SampleTracker.Count++;
				SampleTracker.Weight += Weight;

				if (Blender)
				{
					PCGExData::FPoint Pt(TargetInfos.Index, TargetInfos.IOIndex);
					Union->Add_Unsafe(Pt);
					Union->AddWeight_Unsafe(Pt, Weight);
				}
			};


			if (bSingleSample)
			{
				const PCGExNearestPoint::FSample& TargetInfos = bSampleClosest ? Stats.Closest : Stats.Farthest;
				const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
				ProcessTargetInfos(TargetInfos, Weight);
			}
			else
			{
				for (PCGExNearestPoint::FSample& TargetInfos : Samples)
				{
					const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
					if (Weight == 0) { continue; }
					ProcessTargetInfos(TargetInfos, Weight, DataBlender);
				}
			}

			DataBlender->MergeSingle(Index, Union, OutWeightedPoints, Trackers);


			if (SampleTracker.Weight != 0) // Dodge NaN
			{
				WeightedUp /= SampleTracker.Weight;
				WeightedTransform = PCGExBlend::Div(WeightedTransform, SampleTracker.Weight);
				WeightedDistance /= SampleTracker.Count; // Weighted distance is an average, not a weight T_T
			}

			WeightedUp.Normalize();

			const FVector CWDistance = Origin - WeightedTransform.GetLocation();
			FVector LookAt = CWDistance.GetSafeNormal();

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
				const double D = DistanceWriter->GetValue(i);
				DistanceWriter->SetValue(i, (1 - (D / MaxDistance)) * Settings->DistanceScale);
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const double D = DistanceWriter->GetValue(i);
				DistanceWriter->SetValue(i, (D / MaxDistance) * Settings->DistanceScale);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (UnionBlendOpsManager) { UnionBlendOpsManager->Cleanup(Context); }
		PointDataFacade->WriteFastest(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		(void)PointDataFacade->Source->Gather(SamplingMask);
	}

	void FProcessor::Cleanup()
	{
		TPointsProcessor<FPCGExSampleNearestPointContext, UPCGExSampleNearestPointSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
