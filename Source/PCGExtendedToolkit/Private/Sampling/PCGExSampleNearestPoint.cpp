// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"

#include "PCGExMath.h"
#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendModes.h"
#include "Data/Blending/PCGExBlendOpFactoryProvider.h"
#include "Data/Blending/PCGExBlendOpsManager.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExUnionOpsManager.h"
#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"
#include "Details/PCGExDetailsSettings.h"


#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"
#define PCGEX_NAMESPACE SampleNearestPoint

PCGEX_SETTING_VALUE_IMPL(UPCGExSampleNearestPointSettings, RangeMax, double, RangeMaxInput, RangeMaxAttribute, RangeMax)
PCGEX_SETTING_VALUE_IMPL(UPCGExSampleNearestPointSettings, RangeMin, double, RangeMinInput, RangeMinAttribute, RangeMin)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExSampleNearestPointSettings, LookAtUp, FVector, LookAtUpSelection != EPCGExSampleSource::Constant, LookAtUpSource, LookAtUpConstant)

UPCGExSampleNearestPointSettings::UPCGExSampleNearestPointSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinear; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGEx::SourceTargetsLabel, "The point data set to check against.", Required)

	PCGExMatching::DeclareMatchingRulesInputs(DataMatching, PinProperties);
	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal, BlendingInterface);
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, SampleMethod == EPCGExSampleMethod::BestCandidate ? EPCGPinStatus::Required : EPCGPinStatus::Advanced);

	PCGEX_PIN_FILTERS(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced)

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGExMatching::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	return PinProperties;
}

bool UPCGExSampleNearestPointSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExSorting::SourceSortingRules) { return SampleMethod == EPCGExSampleMethod::BestCandidate; }
	if (InPin->Properties.Label == PCGExDataBlending::SourceBlendingLabel) { return BlendingInterface == EPCGExBlendingInterface::Individual && InPin->EdgeCount() > 0; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

void FPCGExSampleNearestPointContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleNearestPoint)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightOverDistance.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestPoint)

PCGExData::EIOInit UPCGExSampleNearestPointSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleNearestPoint)

bool FPCGExSampleNearestPointElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories, {PCGExFactories::EType::Blending}, false);
	}

	Context->TargetsHandler = MakeShared<PCGExSampling::FTargetsHandler>();
	Context->TargetsHandler->Init(Context, PCGEx::SourceTargetsLabel);

	Context->NumMaxTargets = Context->TargetsHandler->GetMaxNumTargets();
	if (!Context->NumMaxTargets)
	{
		PCGEX_LOG_MISSING_INPUT(Context, FTEXT("No targets (empty datasets)"))
		return false;
	}

	Context->TargetsHandler->SetDistances(Settings->DistanceDetails);

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::FPointSorter>(PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
	}

	Context->TargetsHandler->ForEachPreloader([&](PCGExData::FFacadePreloader& Preloader)
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

bool FPCGExSampleNearestPointElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)
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
				// Prep weights
				if (Settings->WeightMode != EPCGExSampleWeightMode::Distance)
				{
					TSharedPtr<PCGExData::TBuffer<double>> Weight = Target->GetBroadcaster<double>(Settings->WeightAttribute);
					if (!Weight)
					{
						PCGEX_LOG_INVALID_SELECTOR_C(Context, Target Weight, Settings->WeightAttribute)
						bBreak = true;
						return;
					}

					Context->TargetWeights.Add(Weight);
				}

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

		Context->TargetsHandler->StartLoading(Context->GetAsyncManager());
		if (Context->IsWaitingForTasks()) { return false; }
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExSampleNearestPointElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}

namespace PCGExSampleNearestPoint
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
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestPoint::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->bIgnoreSelf) { IgnoreList.Add(PointDataFacade->GetIn()); }

		if (PCGExMatching::FMatchingScope MatchingScope(Context->InitialMainPointsNum, true); !Context->TargetsHandler->PopulateIgnoreList(PointDataFacade->Source, MatchingScope, IgnoreList))
		{
			if (!Context->TargetsHandler->HandleUnmatchedOutput(PointDataFacade, true)) { PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Forward) }
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
			PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_INIT)
		}

		if (!Context->BlendingFactories.IsEmpty())
		{
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, Context->TargetsHandler->GetDistances());
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetsHandler->GetFacades())) { return false; }
			DataBlender = UnionBlendOpsManager;
		}
		else if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic)
		{
			TSet<FName> MissingAttributes;
			PCGExDataBlending::AssembleBlendingDetails(Settings->PointPropertiesBlendingSettings, Settings->TargetAttributes, Context->TargetsHandler->GetFacades(), BlendingDetails, MissingAttributes);

			UnionBlender = MakeShared<PCGExDataBlending::FUnionBlender>(&BlendingDetails, nullptr, Context->TargetsHandler->GetDistances());
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

		RangeMinGetter = Settings->GetValueSettingRangeMin();
		if (!RangeMinGetter->Init(PointDataFacade)) { return false; }

		RangeMaxGetter = Settings->GetValueSettingRangeMax();
		if (!RangeMaxGetter->Init(PointDataFacade)) { return false; }

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExSampleNearestPointContext, UPCGExSampleNearestPointSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestPoint::ProcessPoints);

		const bool bWeightUseAttr = Settings->WeightMode == EPCGExSampleWeightMode::Attribute;
		const bool bWeightUseAttrMult = Settings->WeightMode == EPCGExSampleWeightMode::AttributeMult;
		const bool bSampleClosest = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget;
		const bool bSampleFarthest = Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget;
		const bool bSampleBest = Settings->SampleMethod == EPCGExSampleMethod::BestCandidate;

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

		PCGEX_SCOPE_LOOP(Index)
		{
			Union->Reset();

			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			double RangeMin = FMath::Square(RangeMinGetter->Read(Index));
			double RangeMax = FMath::Square(RangeMaxGetter->Read(Index));

			if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

			if (RangeMax == 0) { Union->Elements.Reserve(Context->NumMaxTargets); }

			const PCGExData::FMutablePoint Point = PointDataFacade->GetOutPoint(Index);
			const FVector Origin = Transforms[Index].GetLocation();

			PCGExData::FElement SinglePick(-1, -1);
			double Det = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? MAX_dbl : MIN_dbl;

			auto SampleSingleTarget = [&](const PCGExData::FConstPoint& Target)
			{
				double DistSquared = Context->TargetsHandler->GetDistSquared(Point, Target);
				if (RangeMax > 0 && (DistSquared < RangeMin || DistSquared > RangeMax)) { return; }
				if (bWeightUseAttr) { DistSquared = Context->TargetWeights[Target.IO]->Read(Target.Index); }
				else if (bWeightUseAttrMult) { DistSquared *= Context->TargetWeights[Target.IO]->Read(Target.Index); }

				bool bReplaceWithCurrent = Union->IsEmpty();

				if (bSampleBest)
				{
					if (SinglePick.Index != -1) { bReplaceWithCurrent = Context->Sorter->Sort(static_cast<PCGExData::FElement>(Target), SinglePick); }
				}
				else if ((bSampleClosest && Det > DistSquared) || (bSampleFarthest && Det < DistSquared))
				{
					bReplaceWithCurrent = true;
				}

				if (bReplaceWithCurrent)
				{
					SinglePick = static_cast<PCGExData::FElement>(Target);
					Det = DistSquared;
					Union->Reset();
					Union->AddWeighted_Unsafe(Target, DistSquared);
				}
			};

			auto SampleMultiTarget = [&](const PCGExData::FConstPoint& Target)
			{
				double DistSquared = Context->TargetsHandler->GetDistSquared(Point, Target);
				if (RangeMax > 0 && (DistSquared < RangeMin || DistSquared > RangeMax)) { return; }
				if (bWeightUseAttr) { DistSquared = Context->TargetWeights[Target.IO]->Read(Target.Index); }
				else if (bWeightUseAttrMult) { DistSquared *= Context->TargetWeights[Target.IO]->Read(Target.Index); }

				Union->AddWeighted_Unsafe(Target, DistSquared);
			};

			if (RangeMax > 0)
			{
				const FBox Box = FBoxCenterAndExtent(Origin, FVector(FMath::Sqrt(RangeMax))).GetBox();
				if (bSingleSample) { Context->TargetsHandler->FindElementsWithBoundsTest(Box, SampleSingleTarget, &IgnoreList); }
				else { Context->TargetsHandler->FindElementsWithBoundsTest(Box, SampleMultiTarget, &IgnoreList); }
			}
			else
			{
				if (bSingleSample) { Context->TargetsHandler->ForEachTargetPoint(SampleSingleTarget, &IgnoreList); }
				else { Context->TargetsHandler->ForEachTargetPoint(SampleMultiTarget, &IgnoreList); }
			}

			if (Union->IsEmpty())
			{
				SamplingFailed(Index);
				continue;
			}

			if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0) { Union->WeightRange = RangeMax; }
			DataBlender->ComputeWeights(Index, Union, OutWeightedPoints);

			FTransform WeightedTransform = FTransform::Identity;
			WeightedTransform.SetScale3D(FVector::ZeroVector);

			FVector WeightedUp = SafeUpVector;
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Source) { WeightedUp = LookAtUpGetter->Read(Index); }

			FVector WeightedSignAxis = FVector::ZeroVector;
			FVector WeightedAngleAxis = FVector::ZeroVector;

			double WeightedDistance = Union->GetSqrtWeightAverage();

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
		PointDataFacade->WriteFastest(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->bPruneFailedSamples) { (void)PointDataFacade->Source->Gather(SamplingMask); }
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExSampleNearestPointContext, UPCGExSampleNearestPointSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
