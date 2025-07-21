// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPath.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestPathElement"
#define PCGEX_NAMESPACE SampleNearestPath

UPCGExSampleNearestPathSettings::UPCGExSampleNearestPathSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinear; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGExPaths::SourcePathsLabel, "The paths to sample.", Required, {})
	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Normal, {})

	if (SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	else
	{
		PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Advanced, {})
	}

	return PinProperties;
}

bool UPCGExSampleNearestPathSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExSorting::SourceSortingRules) { return SampleMethod == EPCGExSampleMethod::BestCandidate; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

void FPCGExSampleNearestPathContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleNearestPath)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightOverDistance.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestPath)

bool FPCGExSampleNearestPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPath)

	PCGEX_FOREACH_FIELD_NEARESTPATH(PCGEX_OUTPUT_VALIDATE_NAME)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	Context->TargetsHandler = MakeShared<PCGExSampling::FTargetsHandler>();
	Context->NumMaxTargets = Context->TargetsHandler->Init(
		Context, PCGExPaths::SourcePathsLabel,
		[&](const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Idx)-> FBox
		{
			const bool bClosedLoop = PCGExPaths::GetClosedLoop(IO->GetIn());

			switch (Settings->SampleInputs)
			{
			default:
			case EPCGExPathSamplingIncludeMode::All:
				break;
			case EPCGExPathSamplingIncludeMode::ClosedLoopOnly:
				if (!bClosedLoop) { return FBox(NoInit); }
				break;
			case EPCGExPathSamplingIncludeMode::OpenLoopsOnly:
				if (bClosedLoop) { return FBox(NoInit); }
				break;
			}

			TSharedPtr<PCGExPaths::FPath> Path = PCGExPaths::MakePolyPath(IO->GetIn(), 1, FVector::UpVector, Settings->HeightInclusion);

			Path->IOIndex = IO->IOIndex;
			Path->Idx = Idx;

			Context->Paths.Add(Path);

			return Path->Bounds;
		});

	Context->NumMaxTargets = Context->TargetsHandler->GetMaxNumTargets();
	if (!Context->NumMaxTargets)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (no input matches criteria)"));
		return false;
	}

	Context->TargetsHandler->SetDistances(Settings->DistanceSettings, Settings->DistanceSettings, false);

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::FPointSorter>(PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
	}

	if (!Context->BlendingFactories.IsEmpty())
	{
		Context->TargetsHandler->ForEachPreloader(
			[&](PCGExData::FFacadePreloader& Preloader)
			{
				PCGExDataBlending::RegisterBuffersDependencies_SourceA(Context, Preloader, Context->BlendingFactories);
			});
	}

	return true;
}

void FPCGExSampleNearestPathElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPath)

	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	Context->RuntimeWeightCurve = Settings->LocalWeightOverDistance;

	if (!Settings->bUseLocalCurve)
	{
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(0, 0);
		Context->RuntimeWeightCurve.EditorCurveData.AddKey(1, 1);
		Context->RuntimeWeightCurve.ExternalCurve = Settings->WeightOverDistance.Get();
	}

	Context->WeightCurve = Context->RuntimeWeightCurve.GetRichCurveConst();
}

bool FPCGExSampleNearestPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGEx::State_FacadePreloading);

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();
		Context->TargetsHandler->TargetsPreloader->OnCompleteCallback = [Settings, Context, WeakHandle]()
		{
			PCGEX_SHARED_CONTEXT_VOID(WeakHandle)

			const bool bError = Context->TargetsHandler->ForEachTarget(
				[&](const TSharedRef<PCGExData::FFacade>& Target, const int32 TargetIndex, bool& bBreak)
				{
					// Prep look up getters
					if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
					{
						// TODO : Preload if relevant
						TSharedPtr<PCGExDetails::TSettingValue<FVector>> LookAtUpGetter = Settings->GetValueSettingLookAtUp();
						if (!LookAtUpGetter->Init(Context, Target, false))
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

			if (Context->Sorter && !Context->Sorter->Init(Context, Context->TargetsHandler->GetFacades()))
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestPath::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleNearestPath::FProcessor>>& NewBatch)
				{
					if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
				}))
			{
				Context->CancelExecution(TEXT("Could not find any paths to split."));
			}
		};

		Context->TargetsHandler->StartLoading(Context->GetAsyncManager());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExSampleNearestPathElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}


namespace PCGExSampleNearestPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestPath::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
		if (Settings->bIgnoreSelf) { IgnoreList.Add(PointDataFacade->GetIn()); }

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
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, Context->TargetsHandler->GetDistances());
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetsHandler->GetFacades())) { return false; }
			DataBlender = UnionBlendOpsManager;
		}

		if (!DataBlender)
		{
			TSharedPtr<PCGExDataBlending::FDummyUnionBlender> DummyUnionBlender = MakeShared<PCGExDataBlending::FDummyUnionBlender>();
			DummyUnionBlender->Init(PointDataFacade, Context->TargetsHandler->GetFacades());
			DataBlender = DummyUnionBlender;
		}

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTPATH(PCGEX_OUTPUT_INIT)
		}

		RangeMinGetter = Settings->GetValueSettingRangeMin();
		if (!RangeMinGetter->Init(Context, PointDataFacade)) { return false; }

		RangeMaxGetter = Settings->GetValueSettingRangeMax();
		if (!RangeMaxGetter->Init(Context, PointDataFacade)) { return false; }

		if (Settings->bSampleSpecificAlpha)
		{
			SampleAlphaGetter = Settings->GetValueSettingSampleAlpha();
			if (!SampleAlphaGetter->Init(Context, PointDataFacade)) { return false; }
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection == EPCGExSampleSource::Source)
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
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
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

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bAnySuccessLocal = false;

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		DataBlender->InitTrackers(Trackers);

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

			auto SampleTarget = [&](const int32 EdgeIndex, const double& Lerp, const TSharedPtr<PCGExPaths::FPath>& InPath)
			{
				const PCGExData::FElement EdgeElement(EdgeIndex, InPath->Idx);
				const PCGExData::FElement A(InPath->Edges[EdgeIndex].Start, InPath->Idx);
				const PCGExData::FElement B(InPath->Edges[EdgeIndex].End, InPath->Idx);

				const bool bIsInside = InPath->IsInsideProjection(Transform);

				if (Settings->bOnlySampleWhenInside && !bIsInside) { return; }

				int32 NumInsideIncrement = 0;
				if (bIsInside) { if (!bOnlyIncrementInsideNumIfClosed || InPath->IsClosedLoop()) { NumInsideIncrement = 1; } }

				const FVector PosA = InPath->GetPos(A.Index);
				const FVector PosB = InPath->GetPos(B.Index);

				const FVector SampleLocation = FMath::Lerp(PosA, PosB, Lerp);

				const FVector ModifiedOrigin = Context->TargetsHandler->GetSourceCenter(Point, Origin, SampleLocation);
				const double DistSquared = FVector::DistSquared(ModifiedOrigin, SampleLocation);

				if (RangeMax > 0 && (DistSquared < RangeMin || DistSquared > RangeMax))
				{
					if (!Settings->bAlwaysSampleWhenInside || !bIsInside) { return; }
				}

				const double Time = (static_cast<double>(EdgeIndex) + Lerp) / static_cast<double>(InPath->NumEdges);

				///////

				if (bSingleSample)
				{
					bool bReplaceWithCurrent = Union->IsEmpty();

					if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
					{
						if (SinglePick.Index != -1) { bReplaceWithCurrent = Context->Sorter->Sort(EdgeElement, SinglePick); }
					}
					else if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget && WeightedDistance > DistSquared)
					{
						bReplaceWithCurrent = true;
					}
					else if (Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget && WeightedDistance < DistSquared)
					{
						bReplaceWithCurrent = true;
					}

					if (bReplaceWithCurrent)
					{
						SinglePick = EdgeElement;
						WeightedDistance = DistSquared;

						// TODO : Adjust dist based on edge lerp
						Union->Reset();
						Union->AddWeighted_Unsafe(A, DistSquared);
						Union->AddWeighted_Unsafe(B, DistSquared);

						NumInside = NumInsideIncrement;
						NumInClosed = bSampledClosedLoop = InPath->IsClosedLoop();

						WeightedTime = Time;
						WeightedSegmentTime = Lerp;
					}
				}
				else
				{
					// TODO : Adjust dist based on edge lerp
					WeightedDistance += DistSquared;
					Union->AddWeighted_Unsafe(A, DistSquared);
					Union->AddWeighted_Unsafe(B, DistSquared);

					if (InPath->IsClosedLoop())
					{
						bSampledClosedLoop = true;
						NumInClosed += NumInsideIncrement;
					}

					WeightedTime += Time;
					WeightedSegmentTime += Lerp;

					NumInside += NumInsideIncrement;
				}
			};


			const FBox QueryBounds = FBox(Origin - FVector(RangeMax), Origin + FVector(RangeMax));

			// First: Sample all possible targets
			if (!Settings->bSampleSpecificAlpha)
			{
				// At closest alpha
				Context->TargetsHandler->FindTargetsWithBoundsTest(
					QueryBounds,
					[&](const PCGEx::FIndexedItem& Target)
					{
						const TSharedPtr<PCGExPaths::FPath> Path = Context->Paths[Target.Index];
						float Lerp = 0;
						const int32 EdgeIndex = Path->GetClosestEdge(Origin, Lerp);
						SampleTarget(EdgeIndex, Lerp, Path);
					}, &IgnoreList);
			}
			else
			{
				// At specific alpha
				const double InputKey = SampleAlphaGetter->Read(Index);
				Context->TargetsHandler->FindTargetsWithBoundsTest(
					QueryBounds,
					[&](const PCGEx::FIndexedItem& Target)
					{
						const TSharedPtr<PCGExPaths::FPath>& Path = Context->Paths[Target.Index];
						double Time = 0;

						switch (Settings->SampleAlphaMode)
						{
						default:
						case EPCGExPathSampleAlphaMode::Alpha:
							Time = InputKey;
							break;
						case EPCGExPathSampleAlphaMode::Time:
							Time = InputKey / Path->NumEdges;
							break;
						case EPCGExPathSampleAlphaMode::Distance:
							Time = InputKey / Path->TotalLength;
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

			FTransform WeightedTransform = FTransform::Identity;
			WeightedTransform.SetScale3D(FVector::ZeroVector);

			FVector WeightedUp = SafeUpVector;
			if (LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

			FVector WeightedSignAxis = FVector::ZeroVector;
			FVector WeightedAngleAxis = FVector::ZeroVector;

			const double NumSampled = Union->Num() * 0.5;
			WeightedDistance /= NumSampled; // We have two points per samples
			WeightedTime /= NumSampled;
			WeightedSegmentTime /= NumSampled;

			double TotalWeight = 0;

			// Post-process weighted points and compute local data
			PCGEx::FOpStats SampleTracker{};
			for (PCGExData::FWeightedPoint& P : OutWeightedPoints)
			{
				const double W = Context->WeightCurve->Eval(P.Weight);

				// Don't remap blending if we use external blend ops; they have their own curve
				//if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic) { P.Weight = W; }

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

				TotalWeight += W;
			}

			// Blend using updated weighted points
			DataBlender->Blend(Index, OutWeightedPoints, Trackers);

			if (TotalWeight != 0) // Dodge NaN
			{
				WeightedUp /= TotalWeight;
				WeightedTransform = PCGExBlend::Div(WeightedTransform, TotalWeight);
			}
			else
			{
				WeightedUp = WeightedUp.GetSafeNormal();
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
			PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
			PCGEX_OUTPUT_VALUE(SegmentTime, Index, WeightedSegmentTime)
			PCGEX_OUTPUT_VALUE(Time, Index, WeightedTime)
			PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
			PCGEX_OUTPUT_VALUE(NumSamples, Index, NumSampled)
			PCGEX_OUTPUT_VALUE(ClosedLoop, Index, bSampledClosedLoop)

			MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), WeightedDistance));
			bAnySuccessLocal = true;
		}

		if (bAnySuccessLocal) { FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1); }
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
