// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestBounds.h"

#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendModes.h"
#include "Data/Blending/PCGExBlendOpsManager.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExUnionOpsManager.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"
#include "Geometry/PCGExGeoPointBox.h"

PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExSampleNearestBoundsSettings, LookAtUp, FVector, LookAtUpSelection != EPCGExSampleSource::Constant, LookAtUpSource, LookAtUpConstant)

#define LOCTEXT_NAMESPACE "PCGExSampleNearestBoundsElement"
#define PCGEX_NAMESPACE SampleNearestBounds

UPCGExSampleNearestBoundsSettings::UPCGExSampleNearestBoundsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightRemap) { WeightRemap = PCGEx::WeightDistributionLinear; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestBoundsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGEx::SourceBoundsLabel, "The bounds data set to check against.", Required)
	PCGExMatching::DeclareMatchingRulesInputs(DataMatching, PinProperties);
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, SampleMethod == EPCGExBoundsSampleMethod::BestCandidate ? EPCGPinStatus::Required : EPCGPinStatus::Advanced);
	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal, BlendingInterface);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSampleNearestBoundsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGExMatching::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	return PinProperties;
}

bool UPCGExSampleNearestBoundsSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExSorting::SourceSortingRules) { return SampleMethod == EPCGExBoundsSampleMethod::BestCandidate; }
	if (InPin->Properties.Label == PCGExDataBlending::SourceBlendingLabel) { return BlendingInterface == EPCGExBlendingInterface::Individual && InPin->EdgeCount() > 0; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

void FPCGExSampleNearestBoundsContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleNearestBounds)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightRemap.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestBounds)

PCGExData::EIOInit UPCGExSampleNearestBoundsSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleNearestBounds)

bool FPCGExSampleNearestBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories, {PCGExFactories::EType::Blending}, false);
	}

	Context->TargetsHandler = MakeShared<PCGExSampling::FTargetsHandler>();
	Context->NumMaxTargets = Context->TargetsHandler->Init(Context, PCGEx::SourceBoundsLabel);

	if (!Context->NumMaxTargets)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No valid bounds"));
		return false;
	}

	Context->DistanceDetails = PCGExDetails::MakeDistances();

	if (Settings->SampleMethod == EPCGExBoundsSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::FPointSorter>(PCGExSorting::GetSortingRules(InContext, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
	}

	Context->TargetsHandler->ForEachPreloader([&](PCGExData::FFacadePreloader& Preloader)
	{
		Context->Clouds.Add(Preloader.GetDataFacade()->GetCloud(Settings->BoundsSource));
		PCGExDataBlending::RegisterBuffersDependencies_SourceA(Context, Preloader, Context->BlendingFactories);
	});


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

bool FPCGExSampleNearestBoundsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGExCommon::State_FacadePreloading);

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();
		Context->TargetsHandler->TargetsPreloader->OnCompleteCallback = [Settings, Context, WeakHandle]()
		{
			PCGEX_SHARED_CONTEXT_VOID(WeakHandle)

			const bool bError = Context->TargetsHandler->ForEachTarget([&](const TSharedRef<PCGExData::FFacade>& Target, const int32 TargetIndex, bool& bBreak)
			{
				// Prep look up getters
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
				{
					// TODO : Preload if relevant
					TSharedPtr<PCGExDetails::TSettingValue<FVector>> LookAtUpGetter = Settings->GetValueSettingLookAtUp();
					if (!LookAtUpGetter->Init(Target, false))
					{
						bBreak = true;
						return;
					}

					Context->TargetLookAtUpGetters.Add(LookAtUpGetter);
				}
			});

			if (bError)
			{
				Context->CancelExecution();
				return;
			}

			Context->TargetsHandler->SetMatchingDetails(Context, &Settings->DataMatching);

			if (Context->Sorter && !Context->Sorter->Init(Context, Context->TargetsHandler->GetFacades()))
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->TargetsHandler->StartLoading(Context->GetTaskManager());
		if (Context->IsWaitingForTasks()) { return false; }
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestBounds::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Settings->bIgnoreSelf) { IgnoreList.Add(PointDataFacade->GetIn()); }
		if (PCGExMatching::FMatchingScope MatchingScope(Context->InitialMainPointsNum, true); !Context->TargetsHandler->PopulateIgnoreList(PointDataFacade->Source, MatchingScope, IgnoreList))
		{
			(void)Context->TargetsHandler->HandleUnmatchedOutput(PointDataFacade, true);
			return false;
		}

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		if (Context->ApplySampling.WantsApply()) { AllocateFor |= EPCGPointNativeProperties::Transform; }
		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_INIT)
		}

		if (!Context->BlendingFactories.IsEmpty())
		{
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, Context->DistanceDetails);
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetsHandler->GetFacades())) { return false; }
			DataBlender = UnionBlendOpsManager;
		}
		else if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic)
		{
			TSet<FName> MissingAttributes;
			PCGExDataBlending::AssembleBlendingDetails(Settings->PointPropertiesBlendingSettings, Settings->TargetAttributes, Context->TargetsHandler->GetFacades(), BlendingDetails, MissingAttributes);

			UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&BlendingDetails, nullptr, Context->DistanceDetails);
			UnionBlender->AddSources(Context->TargetsHandler->GetFacades());
			if (!UnionBlender->Init(Context, PointDataFacade)) { return false; }
			DataBlender = UnionBlender;
		}

		if (!DataBlender)
		{
			TSharedPtr<PCGExDataBlending::FDummyUnionBlender> DummyUnionBlender = MakeShared<PCGExDataBlending::FDummyUnionBlender>();
			DummyUnionBlender->Init(PointDataFacade, Context->TargetsHandler->GetFacades());
			DataBlender = DummyUnionBlender;
		}

		if (Settings->bWriteLookAtTransform)
		{
			if (Settings->LookAtUpSelection != EPCGExSampleSource::Target)
			{
				LookAtUpGetter = Settings->GetValueSettingLookAtUp();
				if (!LookAtUpGetter->Init(PointDataFacade)) { return false; }
			}
		}
		else
		{
			LookAtUpGetter = PCGExDetails::MakeSettingValue(Settings->LookAtUpConstant);
		}

		bSingleSample = Settings->SampleMethod != EPCGExBoundsSampleMethod::WithinRange;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExSampleNearestBoundsContext, UPCGExSampleNearestBoundsSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestBounds::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bLocalAnySuccess = false;

		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;

		DataBlender->InitTrackers(Trackers);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		const TSharedPtr<PCGExSampling::FSampingUnionData> Union = MakeShared<PCGExSampling::FSampingUnionData>();
		Union->IOSet.Reserve(Context->TargetsHandler->Num());
		Union->WeightRange = -2; // Don't remap

		PCGExGeo::FSample CloudSample;

		double DefaultDet = 0;

		switch (Settings->SampleMethod)
		{
		case EPCGExBoundsSampleMethod::BestCandidate: DefaultDet = -1;
			break;
		default: case EPCGExBoundsSampleMethod::ClosestBounds:
		case EPCGExBoundsSampleMethod::SmallestBounds: DefaultDet = MAX_dbl;
			break;
		case EPCGExBoundsSampleMethod::FarthestBounds:
		case EPCGExBoundsSampleMethod::LargestBounds: DefaultDet = MIN_dbl;
			break;
		}

		PCGEX_SCOPE_LOOP(Index)
		{
			Union->Reset();

			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			PCGExData::FElement SinglePick(-1, -1);
			double Det = DefaultDet;

			const PCGExData::FMutablePoint Point = PointDataFacade->GetOutPoint(Index);
			const FVector Origin = Transforms[Index].GetLocation();

			const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Origin, PCGExMath::GetLocalBounds(Point, BoundsSource).GetExtent());

			auto SampleSingle = [&](const PCGExData::FElement& Current, const PCGExGeo::FPointBox* NearbyBox)
			{
				double DetCandidate = Det;
				bool bReplaceWithCurrent = Union->IsEmpty();

				switch (Settings->SampleMethod)
				{
				case EPCGExBoundsSampleMethod::BestCandidate: DetCandidate = NearbyBox->Index;
					if (SinglePick.Index != -1) { bReplaceWithCurrent = Context->Sorter->Sort(Current, SinglePick); }
					else { bReplaceWithCurrent = true; }
					break;
				default: case EPCGExBoundsSampleMethod::ClosestBounds: DetCandidate = CloudSample.Distances.SizeSquared();
					bReplaceWithCurrent = DetCandidate < Det;
					break;
				case EPCGExBoundsSampleMethod::FarthestBounds: DetCandidate = CloudSample.Distances.SizeSquared();
					bReplaceWithCurrent = DetCandidate > Det;
					break;
				case EPCGExBoundsSampleMethod::SmallestBounds: DetCandidate = NearbyBox->RadiusSquared;
					bReplaceWithCurrent = DetCandidate < Det;
					break;
				case EPCGExBoundsSampleMethod::LargestBounds: DetCandidate = NearbyBox->RadiusSquared;
					bReplaceWithCurrent = DetCandidate > Det;
					break;
				}

				if (bReplaceWithCurrent)
				{
					SinglePick = Current;
					Det = DetCandidate;
					Union->Reset();
					Union->AddWeighted_Unsafe(Current, CloudSample.Weight);
				}
			};

			Context->TargetsHandler->FindTargetsWithBoundsTest(BCAE, [&](const PCGExOctree::FItem& Target)
			{
				Context->Clouds[Target.Index]->GetOctree()->FindElementsWithBoundsTest(BCAE, [&](const PCGExGeo::FPointBox* NearbyBox)
				{
					NearbyBox->Sample(Origin, CloudSample);
					if (!CloudSample.bIsInside) { return; }

					const PCGExData::FElement Current(NearbyBox->Index, Target.Index);
					if (bSingleSample) { SampleSingle(Current, NearbyBox); }
					else { Union->AddWeighted_Unsafe(Current, CloudSample.Weight); }
				});
			}, &IgnoreList);

			if (Union->IsEmpty())
			{
				SamplingFailed(Index);
				continue;
			}

			DataBlender->ComputeWeights(Index, Union, OutWeightedPoints);

			FTransform WeightedTransform = FTransform::Identity;
			WeightedTransform.SetScale3D(FVector::ZeroVector);
			FVector WeightedUp = SafeUpVector;
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

			FVector WeightedSignAxis = FVector::ZeroVector;
			FVector WeightedAngleAxis = FVector::ZeroVector;

			// Post-process weighted points and compute local data
			PCGEx::FOpStats SampleTracker{};

			for (PCGExData::FWeightedPoint& P : OutWeightedPoints)
			{
				const double W = Context->WeightCurve->Eval(P.Weight);

				// Don't remap blending if we use external blend ops; they have their own curve
				if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic) { P.Weight = W; }

				SampleTracker.Count++;
				SampleTracker.Weight += W;

				const FTransform& TargetTransform = Context->TargetsHandler->GetPoint(P).GetTransform();
				const FQuat TargetRotation = TargetTransform.GetRotation();

				WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, TargetTransform, W);
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
				{
					PCGExBlend::WeightedAdd(WeightedUp, Context->TargetLookAtUpGetters[P.IO]->Read(P.Index), W);
				}

				WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * W;
				WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * W;
			}

			// Blend using updated weighted points
			DataBlender->Blend(Index, OutWeightedPoints, Trackers);

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

			SamplingMask[Index] = !Union->IsEmpty();
			PCGEX_OUTPUT_VALUE(Success, Index, !Union->IsEmpty())
			PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, LookAtTransform)
			PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? WeightedDistance : WeightedDistance * Settings->DistanceScale)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance * Settings->SignedDistanceScale)
			PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
			PCGEX_OUTPUT_VALUE(NumSamples, Index, SampleTracker.Count)
			PCGEX_OUTPUT_VALUE(SampledIndex, Index, SinglePick.Index)

			MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), WeightedDistance));
			bLocalAnySuccess = true;
		}

		if (bLocalAnySuccess) { FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (Settings->bOutputNormalizedDistance && DistanceWriter)
		{
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

		if (UnionBlendOpsManager) { UnionBlendOpsManager->Cleanup(Context); }
		PointDataFacade->WriteFastest(TaskManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->bPruneFailedSamples) { (void)PointDataFacade->Source->Gather(SamplingMask); }
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExSampleNearestBoundsContext, UPCGExSampleNearestBoundsSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
