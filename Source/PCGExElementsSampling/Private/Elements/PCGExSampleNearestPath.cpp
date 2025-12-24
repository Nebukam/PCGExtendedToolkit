// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExSampleNearestPath.h"

#include "Blenders/PCGExUnionOpsManager.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExBlendOpsManager.h"
#include "Core/PCGExOpStats.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExBlendingDetails.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "Helpers/PCGExTargetsHandler.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsCommon.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Paths/PCGExPolyPath.h"
#include "Sampling/PCGExSamplingHelpers.h"
#include "Sampling/PCGExSamplingUnionData.h"
#include "Sorting/PCGExPointSorter.h"
#include "Sorting/PCGExSortingDetails.h"
#include "Types/PCGExTypes.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestPathElement"
#define PCGEX_NAMESPACE SampleNearestPath

PCGEX_SETTING_VALUE_IMPL(UPCGExSampleNearestPathSettings, RangeMin, double, RangeMinInput, RangeMinAttribute, RangeMin)
PCGEX_SETTING_VALUE_IMPL(UPCGExSampleNearestPathSettings, RangeMax, double, RangeMaxInput, RangeMaxAttribute, RangeMax)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExSampleNearestPathSettings, SampleAlpha, double, bSampleSpecificAlpha, SampleAlphaAttribute, SampleAlphaConstant)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExSampleNearestPathSettings, LookAtUp, FVector, LookAtUpSelection != EPCGExSampleSource::Constant, LookAtUpSource, LookAtUpConstant)

UPCGExSampleNearestPathSettings::UPCGExSampleNearestPathSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGExCurves::WeightDistributionLinear; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGExPaths::Labels::SourcePathsLabel, "The paths to sample.", Required)
	PCGExMatching::Helpers::DeclareMatchingRulesInputs(DataMatching, PinProperties);
	PCGExBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal);
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, SampleMethod == EPCGExSampleMethod::BestCandidate ? EPCGPinStatus::Required : EPCGPinStatus::Advanced);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSampleNearestPathSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGExMatching::Helpers::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	return PinProperties;
}

bool UPCGExSampleNearestPathSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExSorting::Labels::SourceSortingRules) { return SampleMethod == EPCGExSampleMethod::BestCandidate; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestPath)

PCGExData::EIOInit UPCGExSampleNearestPathSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleNearestPath)

bool FPCGExSampleNearestPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPath)

	PCGEX_FOREACH_FIELD_NEARESTPATH(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(Context, PCGExBlending::Labels::SourceBlendingLabel, Context->BlendingFactories, {PCGExFactories::EType::Blending}, false);

	Context->TargetsHandler = MakeShared<PCGExMatching::FTargetsHandler>();
	Context->NumMaxTargets = Context->TargetsHandler->Init(Context, PCGExPaths::Labels::SourcePathsLabel, [&](const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Idx)-> FBox
	{
		if (IO->GetNum() < 2) { return FBox(ForceInit); }

		const bool bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(IO->GetIn());

		switch (Settings->SampleInputs)
		{
		default: case EPCGExPathSamplingIncludeMode::All: break;
		case EPCGExPathSamplingIncludeMode::ClosedLoopOnly: if (!bClosedLoop) { return FBox(ForceInit); }
			break;
		case EPCGExPathSamplingIncludeMode::OpenLoopsOnly: if (bClosedLoop) { return FBox(ForceInit); }
			break;
		}

		// TODO : We could support per-point project here but ugh
		TSharedPtr<PCGExPaths::FPolyPath> Path = MakeShared<PCGExPaths::FPolyPath>(IO, Settings->ProjectionDetails, 1, Settings->HeightInclusion);
		Path->OffsetProjection(Settings->InclusionOffset);

		if (!Path->Bounds.IsValid) { return FBox(ForceInit); }

		Path->IOIndex = IO->IOIndex;
		Path->Idx = Idx;

		Context->Paths.Add(Path);

		return Path->Bounds;
	});

	Context->NumMaxTargets = Context->TargetsHandler->GetMaxNumTargets();
	if (!Context->NumMaxTargets)
	{
		PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("No targets (no input matches criteria)"))
		return false;
	}

	Context->TargetsHandler->SetDistances(Settings->DistanceSettings, Settings->DistanceSettings, false);

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::FSorter>(PCGExSorting::GetSortingRules(Context, PCGExSorting::Labels::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
	}

	if (!Context->BlendingFactories.IsEmpty())
	{
		Context->TargetsHandler->ForEachPreloader([&](PCGExData::FFacadePreloader& Preloader)
		{
			PCGExBlending::RegisterBuffersDependencies_SourceA(Context, Preloader, Context->BlendingFactories);
		});
	}

	Context->WeightCurve = Settings->WeightCurveLookup.MakeLookup(
		Settings->bUseLocalCurve, Settings->LocalWeightOverDistance, Settings->WeightOverDistance,
		[](FRichCurve& CurveData)
		{
			CurveData.AddKey(0, 0);
			CurveData.AddKey(1, 1);
		});

	return true;
}

bool FPCGExSampleNearestPathElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGExCommon::States::State_FacadePreloading);

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
				Context->CancelExecution(TEXT("LookUp Attribute on Targets is invalid."));
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
				Context->CancelExecution(TEXT("Could not find any paths to split."));
			}
		};

		Context->TargetsHandler->StartLoading(Context->GetTaskManager());
		if (Context->IsWaitingForTasks()) { return false; }
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleNearestPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestPath::Process)

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }


		if (Settings->bIgnoreSelf) { IgnoreList.Add(PointDataFacade->GetIn()); }
		if (PCGExMatching::FScope MatchingScope(Context->InitialMainPointsNum, true); !Context->TargetsHandler->PopulateIgnoreList(PointDataFacade->Source, MatchingScope, IgnoreList))
		{
			(void)Context->TargetsHandler->HandleUnmatchedOutput(PointDataFacade, true);
			return false;
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PCGEX_INIT_IO)
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		}

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;

		if (Context->ApplySampling.WantsApply())
		{
			AllocateFor |= EPCGPointNativeProperties::Transform;
		}

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		if (Settings->SampleInputs != EPCGExPathSamplingIncludeMode::All)
		{
			bOnlySignIfClosed = Settings->bOnlySignIfClosed;
			bOnlyIncrementInsideNumIfClosed = Settings->bOnlyIncrementInsideNumIfClosed;
		}
		else
		{
			bOnlySignIfClosed = false;
			bOnlyIncrementInsideNumIfClosed = false;
		}

		SafeUpVector = Settings->LookAtUpConstant;

		if (!Context->BlendingFactories.IsEmpty())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(InitBlendingFactories)
			UnionBlendOpsManager = MakeShared<PCGExBlending::FUnionOpsManager>(&Context->BlendingFactories, Context->TargetsHandler->GetDistances());
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetsHandler->GetFacades())) { return false; }
			DataBlender = UnionBlendOpsManager;
		}

		if (!DataBlender)
		{
			TSharedPtr<PCGExBlending::FDummyUnionBlender> DummyUnionBlender = MakeShared<PCGExBlending::FDummyUnionBlender>();
			DummyUnionBlender->Init(PointDataFacade, Context->TargetsHandler->GetFacades());
			DataBlender = DummyUnionBlender;
		}

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTPATH(PCGEX_OUTPUT_INIT)
		}

		RangeMinGetter = Settings->GetValueSettingRangeMin();
		if (!RangeMinGetter->Init(PointDataFacade)) { return false; }

		RangeMaxGetter = Settings->GetValueSettingRangeMax();
		if (!RangeMaxGetter->Init(PointDataFacade)) { return false; }

		if (Settings->bSampleSpecificAlpha)
		{
			SampleAlphaGetter = Settings->GetValueSettingSampleAlpha();
			if (!SampleAlphaGetter->Init(PointDataFacade)) { return false; }
		}

		if (Settings->LookAtUpSelection == EPCGExSampleSource::Source)
		{
			LookAtUpGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->LookAtUpSource, true);
			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("LookAtUp is invalid.")); }
		}

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bClosestSample = Settings->SampleMethod != EPCGExSampleMethod::FarthestTarget;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExSampleNearestPathContext, UPCGExSampleNearestPathSettings>::PrepareLoopScopesForPoints(Loops);
		MaxSampledDistanceScoped = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::SamplingFailed(const int32 Index)
	{
		SamplingMask[Index] = false;

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		const double FailSafeDist = RangeMaxGetter->Read(Index);
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, InTransforms[Index])
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, InTransforms[Index])
		PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? FailSafeDist : FailSafeDist * Settings->DistanceScale)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist * Settings->SignedDistanceScale)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, FVector(FailSafeDist))
		PCGEX_OUTPUT_VALUE(Angle, Index, 0)
		PCGEX_OUTPUT_VALUE(SegmentTime, Index, -1)
		PCGEX_OUTPUT_VALUE(Time, Index, -1)
		PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
		PCGEX_OUTPUT_VALUE(ClosedLoop, Index, false)
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestPath::ProcessPoints);

		const bool bSampleClosest = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget;
		const bool bSampleFarthest = Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget;
		const bool bSampleBest = Settings->SampleMethod == EPCGExSampleMethod::BestCandidate;

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bAnySuccessLocal = false;

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		DataBlender->InitTrackers(Trackers);

		const TSharedPtr<PCGExSampling::FSampingUnionData> Union = MakeShared<PCGExSampling::FSampingUnionData>();
		Union->Reserve(Context->TargetsHandler->Num());

		PCGEX_SCOPE_LOOP(Index)
		{
			Union->Reset();

			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			int32 NumInside = 0;
			int32 NumInClosed = 0;

			bool bSampledClosedLoop = false;

			double RangeMin = FMath::Square(RangeMinGetter->Read(Index));
			double RangeMax = FMath::Square(RangeMaxGetter->Read(Index));

			if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

			if (RangeMax == 0) { Union->Elements.Reserve(Context->NumMaxTargets); }

			PCGExData::FConstPoint Point = PointDataFacade->GetInPoint(Index);
			const FTransform& Transform = InTransforms[Index];
			FVector Origin = Transform.GetLocation();

			PCGExData::FElement SinglePick(-1, -1);
			double WeightedDistance = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? MAX_dbl : MIN_dbl;

			double WeightedTime = 0;
			double WeightedSegmentTime = 0;

			auto SampleSingle = [&](const PCGExData::FElement& EdgeElement, const double DistSquared, const PCGExData::FElement& A, const PCGExData::FElement& B, const double Time, const double Lerp, const int32 NumInsideIncrement, const bool bClosedLoop)
			{
				bool bReplaceWithCurrent = Union->IsEmpty();

				if (bSampleBest)
				{
					if (SinglePick.Index != -1) { bReplaceWithCurrent = Context->Sorter->Sort(EdgeElement, SinglePick); }
				}
				else if ((bSampleClosest && WeightedDistance > DistSquared) || (bSampleFarthest && WeightedDistance < DistSquared))
				{
					bReplaceWithCurrent = true;
				}

				if (bReplaceWithCurrent)
				{
					SinglePick = EdgeElement;
					WeightedDistance = FMath::Square(DistSquared);

					// TODO : Adjust dist based on edge lerp
					Union->Reset();
					Union->AddWeighted_Unsafe(A, DistSquared);
					Union->AddWeighted_Unsafe(B, DistSquared);

					NumInside = NumInsideIncrement;
					NumInClosed = bSampledClosedLoop = bClosedLoop;

					WeightedTime = Time;
					WeightedSegmentTime = Lerp;
				}
			};

			auto SampleMulti = [&](const PCGExData::FElement& EdgeElement, const double DistSquared, const PCGExData::FElement& A, const PCGExData::FElement& B, const double Time, const double Lerp, const int32 NumInsideIncrement, const bool bClosedLoop)
			{
				// TODO : Adjust dist based on edge lerp
				WeightedDistance += FMath::Square(DistSquared);
				Union->AddWeighted_Unsafe(A, DistSquared);
				Union->AddWeighted_Unsafe(B, DistSquared);

				if (bClosedLoop)
				{
					bSampledClosedLoop = true;
					NumInClosed += NumInsideIncrement;
				}

				WeightedTime += Time;
				WeightedSegmentTime += Lerp;

				NumInside += NumInsideIncrement;
			};

			auto SampleTarget = [&](const int32 EdgeIndex, const double& Lerp, const TSharedPtr<PCGExPaths::FPolyPath>& InPath)
			{
				PCGExData::FElement EdgeElement;
				PCGExData::FElement A;
				PCGExData::FElement B;
				InPath->GetEdgeElements(EdgeIndex, EdgeElement, A, B);

				const bool bClosedLoop = InPath->IsClosedLoop();
				const bool bIsInside = InPath->IsInsideProjection(Transform.GetLocation());

				if (Settings->bOnlySampleWhenInside && !bIsInside) { return; }

				const int32 NumInsideIncrement = bIsInside && (!bOnlyIncrementInsideNumIfClosed || bClosedLoop);
				const FVector SampleLocation = FMath::Lerp(InPath->GetPos(A.Index), InPath->GetPos(B.Index), Lerp);
				const FVector ModifiedOrigin = Context->TargetsHandler->GetSourceCenter(Point, Origin, SampleLocation);
				const double DistSquared = FVector::DistSquared(ModifiedOrigin, SampleLocation);

				if (RangeMax > 0
					&& (DistSquared < RangeMin || DistSquared > RangeMax)
					&& (!Settings->bAlwaysSampleWhenInside || !bIsInside))
				{
					return;
				}

				const double Time = (static_cast<double>(EdgeIndex) + Lerp) / static_cast<double>(InPath->NumEdges);

				if (bSingleSample) { SampleSingle(EdgeElement, DistSquared, A, B, Time, Lerp, NumInsideIncrement, bClosedLoop); }
				else { SampleMulti(EdgeElement, DistSquared, A, B, Time, Lerp, NumInsideIncrement, bClosedLoop); }
			};


			const FBox QueryBounds = FBox(Origin - FVector(RangeMax), Origin + FVector(RangeMax));

			// First: Sample all possible targets
			if (!Settings->bSampleSpecificAlpha)
			{
				// At closest alpha
				Context->TargetsHandler->FindTargetsWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Target)
				{
					if (!Context->Paths.IsValidIndex(Target.Index)) { return; } // TODO : Look into why there's a discrepency between paths & targets

					const TSharedPtr<PCGExPaths::FPolyPath> Path = Context->Paths[Target.Index];
					float Lerp = 0;
					const int32 EdgeIndex = Path->GetClosestEdge(Origin, Lerp);
					SampleTarget(EdgeIndex, Lerp, Path);
				}, &IgnoreList);
			}
			else
			{
				// At specific alpha
				const double InputKey = SampleAlphaGetter->Read(Index);
				Context->TargetsHandler->FindTargetsWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Target)
				{
					if (!Context->Paths.IsValidIndex(Target.Index)) { return; } // TODO : Look into why there's a discrepency between paths & targets

					const TSharedPtr<PCGExPaths::FPolyPath>& Path = Context->Paths[Target.Index];
					double Time = 0;

					switch (Settings->SampleAlphaMode)
					{
					default: case EPCGExPathSampleAlphaMode::Alpha: Time = InputKey;
						break;
					case EPCGExPathSampleAlphaMode::Time: Time = InputKey / Path->NumEdges;
						break;
					case EPCGExPathSampleAlphaMode::Distance: Time = InputKey / Path->TotalLength;
						break;
					}

					if (Settings->bWrapClosedLoopAlpha && Path->IsClosedLoop()) { Time = PCGExMath::Tile(Time, 0.0, 1.0); }

					float Lerp = 0;
					const int32 EdgeIndex = Path->GetClosestEdge(Time, Lerp);

					SampleTarget(EdgeIndex, Lerp, Path);
				});
			}

			if (Union->IsEmpty())
			{
				SamplingFailed(Index);
				continue;
			}

			if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0) { Union->WeightRange = RangeMax; }
			DataBlender->ComputeWeights(Index, Union, OutWeightedPoints);

			FVector WeightedUp = LookAtUpGetter ? LookAtUpGetter->Read(Index).GetSafeNormal() : SafeUpVector;
			FTransform WeightedTransform = InTransforms[Index];
			FVector WeightedSignAxis = FVector::ZeroVector;
			FVector WeightedAngleAxis = FVector::ZeroVector;

			if (!Settings->bWeightFromOriginalTransform)
			{
				WeightedTransform = FTransform::Identity;
				WeightedTransform.SetScale3D(FVector::ZeroVector);
			}
			else
			{
				WeightedTransform = InTransforms[Index];
			}

			const double NumSampled = Union->Num() * 0.5;
			WeightedDistance /= NumSampled; // We have two points per samples
			WeightedTime /= NumSampled;
			WeightedSegmentTime /= NumSampled;

			// Post-process weighted points and compute local data
			PCGEx::FOpStats SampleTracker{};
			for (PCGExData::FWeightedPoint& P : OutWeightedPoints)
			{
				const double W = Context->WeightCurve->Eval(P.Weight);

				// Don't remap blending if we use external blend ops; they have their own curve
				//if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic) { P.Weight = W; }

				SampleTracker.Count++;
				SampleTracker.TotalWeight += W;

				const FTransform& TargetTransform = Context->TargetsHandler->GetPoint(P).GetTransform();
				const FQuat TargetRotation = TargetTransform.GetRotation();

				WeightedTransform = PCGExTypeOps::FTypeOps<FTransform>::WeightedAdd(WeightedTransform, TargetTransform, W);

				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
				{
					WeightedUp = PCGExTypeOps::FTypeOps<FVector>::WeightedAdd(WeightedUp, Context->TargetLookAtUpGetters[P.IO]->Read(P.Index), W);
				}

				WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * W;
				WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * W;
			}

			// Blend using updated weighted points
			DataBlender->Blend(Index, OutWeightedPoints, Trackers);

			if (SampleTracker.TotalWeight != 0) // Dodge NaN
			{
				WeightedUp = PCGExTypeOps::FTypeOps<FVector>::NormalizeWeight(WeightedUp, SampleTracker.TotalWeight);
				WeightedTransform = PCGExTypeOps::FTypeOps<FTransform>::NormalizeWeight(WeightedTransform, SampleTracker.TotalWeight);
			}
			else
			{
				WeightedTransform = InTransforms[Index];
			}

			WeightedUp.Normalize();

			const FVector CWDistance = Origin - WeightedTransform.GetLocation();
			FVector LookAt = CWDistance.GetSafeNormal();

			FTransform LookAtTransform = PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign);
			if (Context->ApplySampling.WantsApply())
			{
				PCGExData::FMutablePoint MutablePoint = PointDataFacade->GetOutPoint(Index);
				Context->ApplySampling.Apply(MutablePoint, WeightedTransform, LookAtTransform);
			}

			SamplingMask[Index] = !Union->IsEmpty();
			PCGEX_OUTPUT_VALUE(Success, Index, !Union->IsEmpty())
			PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, LookAtTransform)
			PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? WeightedDistance : WeightedDistance * Settings->DistanceScale)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, (!bOnlySignIfClosed || NumInClosed > 0) ? FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance : WeightedDistance * Settings->SignedDistanceScale)
			PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExTypes::Abs(CWDistance) : CWDistance)
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::Helpers::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
			PCGEX_OUTPUT_VALUE(SegmentTime, Index, WeightedSegmentTime)
			PCGEX_OUTPUT_VALUE(Time, Index, WeightedTime)
			PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
			PCGEX_OUTPUT_VALUE(NumSamples, Index, NumSampled)
			PCGEX_OUTPUT_VALUE(ClosedLoop, Index, bSampledClosedLoop)

			MaxSampledDistanceScoped->Set(Scope, FMath::Max(MaxSampledDistanceScoped->Get(Scope), WeightedDistance));
			bAnySuccessLocal = true;
		}

		if (bAnySuccessLocal) { FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (Settings->bOutputNormalizedDistance && DistanceWriter)
		{
			MaxSampledDistance = MaxSampledDistanceScoped->Max();

			const int32 NumPoints = PointDataFacade->GetNum();

			if (Settings->bOutputOneMinusDistance)
			{
				const double InvMaxDist = 1.0 / MaxSampledDistance;
				const double Scale = Settings->DistanceScale;

				for (int i = 0; i < NumPoints; i++)
				{
					const double D = DistanceWriter->GetValue(i);
					DistanceWriter->SetValue(i, (1.0 - D * InvMaxDist) * Scale);
				}
			}
			else
			{
				const double Scale = (1.0 / MaxSampledDistance) * Settings->DistanceScale;

				for (int i = 0; i < NumPoints; i++)
				{
					const double D = DistanceWriter->GetValue(i);
					DistanceWriter->SetValue(i, D * Scale);
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
		TProcessor<FPCGExSampleNearestPathContext, UPCGExSampleNearestPathSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
