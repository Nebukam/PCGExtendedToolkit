// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleInsidePath.h"

#include "Data/Matching/PCGExMatchRuleFactoryProvider.h"
#include "Misc/PCGExDiscardByPointCount.h"


#define LOCTEXT_NAMESPACE "PCGExSampleInsidePathElement"
#define PCGEX_NAMESPACE SampleInsidePath

UPCGExSampleInsidePathSettings::UPCGExSampleInsidePathSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinear; }
}

FName UPCGExSampleInsidePathSettings::GetMainInputPin() const { return PCGExPaths::SourcePathsLabel; }

FName UPCGExSampleInsidePathSettings::GetMainOutputPin() const { return PCGExPaths::OutputPathsLabel; }

TArray<FPCGPinProperties> UPCGExSampleInsidePathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGEx::SourceTargetsLabel, "The points to sample.", Required, {})
	PCGExMatching::DeclareMatchingRulesInputs(DataMatching, PinProperties);
	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal);
	PCGExSorting::DeclareSortingRulesInputs(PinProperties, SampleMethod == EPCGExSampleMethod::BestCandidate ? EPCGPinStatus::Required : EPCGPinStatus::Advanced);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSampleInsidePathSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (OutputMode == EPCGExSampleInsidePathOutput::Split) { PCGEX_PIN_POINTS(PCGExDiscardByPointCount::OutputDiscardedLabel, "Discard inputs are paths that failed to sample any points, despite valid targets.", Normal, {}) }
	PCGExMatching::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	return PinProperties;
}

bool UPCGExSampleInsidePathSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExSorting::SourceSortingRules) { return SampleMethod == EPCGExSampleMethod::BestCandidate; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

void FPCGExSampleInsidePathContext::RegisterAssetDependencies()
{
	PCGEX_SETTINGS_LOCAL(SampleInsidePath)

	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	AddAssetDependency(Settings->WeightOverDistance.ToSoftObjectPath());
}

PCGEX_INITIALIZE_ELEMENT(SampleInsidePath)

bool FPCGExSampleInsidePathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleInsidePath)

	PCGEX_FOREACH_FIELD_INSIDEPATH(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->RangeMinInput != EPCGExInputValueType::Constant)
	{
		if (!PCGExHelpers::IsDataDomainAttribute(Settings->RangeMinAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Min Range attribute must be on the @Data domain"));
			return false;
		}
	}

	if (Settings->RangeMaxInput != EPCGExInputValueType::Constant)
	{
		if (!PCGExHelpers::IsDataDomainAttribute(Settings->RangeMaxAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Max Range attribute must be on the @Data domain"));
			return false;
		}
	}

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	Context->TargetsHandler = MakeShared<PCGExSampling::FTargetsHandler>();
	Context->NumMaxTargets = Context->TargetsHandler->Init(
		Context, PCGEx::SourceTargetsLabel,
		[&](const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Idx)-> FBox
		{
			const bool bClosedLoop = PCGExPaths::GetClosedLoop(IO->GetIn());

			switch (Settings->ProcessInputs)
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

			return IO->GetIn()->GetBounds();
		});

	Context->NumMaxTargets = Context->TargetsHandler->GetMaxNumTargets();
	if (!Context->NumMaxTargets)
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (no input matches criteria)"));
		return false;
	}

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

void FPCGExSampleInsidePathElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(SampleInsidePath)

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

bool FPCGExSampleInsidePathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleInsidePathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleInsidePath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGEx::State_FacadePreloading);

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();
		Context->TargetsHandler->TargetsPreloader->OnCompleteCallback = [Settings, Context, WeakHandle]()
		{
			PCGEX_SHARED_CONTEXT_VOID(WeakHandle)
			if (Context->Sorter && !Context->Sorter->Init(Context, Context->TargetsHandler->GetFacades()))
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			Context->TargetsHandler->SetMatchingDetails(Context, &Settings->DataMatching);

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleInsidePath::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleInsidePath::FProcessor>>& NewBatch)
				{
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

bool FPCGExSampleInsidePathElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}


namespace PCGExSampleInsidePath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleInsidePath::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }


		if (Settings->bIgnoreSelf) { IgnoreList.Add(PointDataFacade->GetIn()); }
		if (PCGExMatching::FMatchingScope MatchingScope(Context->InitialMainPointsNum, true);
			!Context->TargetsHandler->PopulateIgnoreList(PointDataFacade->Source, MatchingScope, IgnoreList))
		{
			if (!Context->TargetsHandler->HandleUnmatchedOutput(PointDataFacade, true)) { PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Forward) }
			return false;
		}

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		Path = PCGExPaths::MakePolyPath(PointDataFacade, 1, Settings->ProjectionDetails, Settings->HeightInclusion);

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		DistanceDetails = PCGExDetails::MakeDistances();

		if (Settings->ProcessInputs != EPCGExPathSamplingIncludeMode::All)
		{
			bOnlyIncrementInsideNumIfClosed = Settings->bOnlyIncrementInsideNumIfClosed;
		}
		else
		{
			bOnlyIncrementInsideNumIfClosed = false;
		}

		if (!Context->BlendingFactories.IsEmpty())
		{
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, DistanceDetails);
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
			PCGEX_FOREACH_FIELD_INSIDEPATH(PCGEX_OUTPUT_INIT)
		}

		if (!PCGExDataHelpers::TryGetSettingDataValue(Context, PointDataFacade->GetIn(), Settings->RangeMinInput, Settings->RangeMinAttribute, Settings->RangeMin, RangeMin)) { return false; }
		if (!PCGExDataHelpers::TryGetSettingDataValue(Context, PointDataFacade->GetIn(), Settings->RangeMaxInput, Settings->RangeMaxAttribute, Settings->RangeMax, RangeMax)) { return false; }

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }


		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bClosestSample = Settings->SampleMethod != EPCGExSampleMethod::FarthestTarget;

		SampleBox = PointDataFacade->GetIn()->GetBounds().ExpandBy(RangeMax);

		ProcessPath();

		return true;
	}

	void FProcessor::ProcessPath()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleInsidePath::ProcessPath);

		constexpr int32 Index = 0; // Only support writing to @Data domain, otherwise will write data to the first point of the path

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		DataBlender->InitTrackers(Trackers);

		const TSharedPtr<PCGExSampling::FSampingUnionData> Union = MakeShared<PCGExSampling::FSampingUnionData>();
		Union->IOSet.Reserve(Context->TargetsHandler->Num());

		Union->Reset();

		int32 NumInside = 0;
		const double RangeMinSquared = FMath::Square(RangeMin);
		const double RangeMaxSquared = FMath::Square(RangeMax);

		if (RangeMax == 0) { Union->Elements.Reserve(Context->NumMaxTargets); }

		PCGExData::FElement SinglePick(-1, -1);
		double WeightedDistance = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? MAX_dbl : MIN_dbl;

		double WeightedTime = 0;
		double WeightedSegmentTime = 0;

		auto SampleTarget = [&](const PCGExData::FConstPoint& Target)
		{
			const FTransform& Transform = Target.GetTransform();
			const FVector SampleLocation = Transform.GetLocation();

			const bool bIsInside = Path->IsInsideProjection(Transform.GetLocation());

			if (Settings->bOnlySampleWhenInside && !bIsInside) { return; }

			int32 NumInsideIncrement = 0;
			if (bIsInside) { if (!bOnlyIncrementInsideNumIfClosed || Path->IsClosedLoop()) { NumInsideIncrement = 1; } }

			float Alpha = 0;
			const int32 EdgeIndex = Path->GetClosestEdge(SampleLocation, Alpha);

			const FVector PathLocation = FMath::Lerp(Path->GetPos(EdgeIndex), Path->GetPos(EdgeIndex + 1), Alpha);
			const double DistSquared = FVector::DistSquared(PathLocation, SampleLocation);

			if (RangeMax > 0 && (DistSquared < RangeMinSquared || DistSquared > RangeMaxSquared))
			{
				if (!Settings->bAlwaysSampleWhenInside || !bIsInside) { return; }
			}

			const double Time = (static_cast<double>(EdgeIndex) + Alpha) / static_cast<double>(Path->NumEdges);

			///////

			if (bSingleSample)
			{
				bool bReplaceWithCurrent = Union->IsEmpty();

				if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
				{
					if (SinglePick.Index != -1) { bReplaceWithCurrent = Context->Sorter->Sort(static_cast<PCGExData::FElement>(Target), SinglePick); }
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
					SinglePick = Target;
					WeightedDistance = DistSquared;

					Union->Reset();
					Union->AddWeighted_Unsafe(Target, DistSquared);

					NumInside = NumInsideIncrement;

					WeightedTime = Time;
					WeightedSegmentTime = Alpha;
				}
			}
			else
			{
				// TODO : Adjust dist based on edge lerp
				WeightedDistance += DistSquared;
				Union->AddWeighted_Unsafe(Target, DistSquared);

				WeightedTime += Time;
				WeightedSegmentTime += Alpha;

				NumInside += NumInsideIncrement;
			}
		};

		Context->TargetsHandler->FindElementsWithBoundsTest(
			SampleBox, SampleTarget, &IgnoreList);

		if (Union->IsEmpty())
		{
			SamplingFailed(Index);
			return;
		}

		if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0) { Union->WeightRange = RangeMaxSquared; }
		DataBlender->ComputeWeights(Index, Union, OutWeightedPoints);

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);

		NumSampled = Union->Num();
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

			WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, TargetTransform, W);

			TotalWeight += W;
		}

		// Blend using updated weighted points
		DataBlender->Blend(Index, OutWeightedPoints, Trackers);

		if (TotalWeight != 0) // Dodge NaN
		{
			WeightedTransform = PCGExBlend::Div(WeightedTransform, TotalWeight);
		}
		else
		{
			WeightedTransform = InTransforms[Index];
		}

		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, NumSampled)

		bAnySuccess = true;
	}

	void FProcessor::SamplingFailed(const int32 Index)
	{
		if (NumSampled == 0 && Settings->OutputMode == EPCGExSampleInsidePathOutput::SuccessOnly)
		{
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		const double FailSafeDist = RangeMax;
		PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
	}

	void FProcessor::CompleteWork()
	{
		if (NumSampled == 0 && Settings->OutputMode == EPCGExSampleInsidePathOutput::SuccessOnly) { return; }

		for (const TSharedPtr<PCGExData::IBuffer>& Buffer : PointDataFacade->Buffers) { if (Buffer->IsWritable()) { Buffer->bResetWithFirstValue = true; } }

		if (UnionBlendOpsManager) { UnionBlendOpsManager->Cleanup(Context); }

		PointDataFacade->WriteFastest(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }

		if (NumSampled == 0 && Settings->OutputMode == EPCGExSampleInsidePathOutput::Split)
		{
			PointDataFacade->Source->OutputPin = PCGExDiscardByPointCount::OutputDiscardedLabel;
		}
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExSampleInsidePathContext, UPCGExSampleInsidePathSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
