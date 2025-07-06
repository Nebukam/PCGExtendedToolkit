// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleInsidePath.h"


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

	PCGEX_FOREACH_FIELD_InsidePath(PCGEX_OUTPUT_VALIDATE_NAME)

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

	TSharedPtr<PCGExData::FPointIOCollection> Targets = MakeShared<PCGExData::FPointIOCollection>(
		Context, PCGEx::SourceTargetsLabel, PCGExData::EIOInit::NoInit, true);

	if (Targets->IsEmpty())
	{
		if (!Settings->bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (empty datasets)")); }
		return false;
	}

	Context->TargetFacades.Reserve(Targets->Pairs.Num());

	FBox OctreeBounds = FBox(ForceInit);

	for (const TSharedPtr<PCGExData::FPointIO>& IO : Targets->Pairs)
	{
		const bool bClosedLoop = PCGExPaths::GetClosedLoop(IO->GetIn());

		switch (Settings->ProcessInputs)
		{
		default:
		case EPCGExPathSamplingIncludeMode::All:
			break;
		case EPCGExPathSamplingIncludeMode::ClosedLoopOnly:
			if (!bClosedLoop) { continue; }
			break;
		case EPCGExPathSamplingIncludeMode::OpenLoopsOnly:
			if (bClosedLoop) { continue; }
			break;
		}

		TSharedPtr<PCGExData::FFacade> TargetFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
		TargetFacade->Idx = Context->TargetFacades.Num();

		Context->TargetFacades.Add(TargetFacade.ToSharedRef());
		Context->TargetOctrees.Add(&IO->GetIn()->GetPointOctree());

		OctreeBounds += IO->GetIn()->GetBounds();
	}

	if (Context->TargetFacades.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (no input matches criteria)"));
		return false;
	}

	Context->DistanceDetails = PCGExDetails::MakeDistances(Settings->DistanceSettings, Settings->DistanceSettings);

	Context->TargetsOctree = MakeShared<PCGEx::FIndexedItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
	for (int i = 0; i < Context->TargetFacades.Num(); ++i)
	{
		const TSharedPtr<PCGExData::FFacade> TargetIO = Context->TargetFacades[i];
		Context->NumMaxTargets += TargetIO->GetNum();
		Context->TargetsOctree->AddElement(PCGEx::FIndexedItem(i, TargetIO->GetIn()->GetBounds()));
	}

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::FPointSorter>(PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
	}

	Context->TargetsPreloader = MakeShared<PCGExData::FMultiFacadePreloader>(Context->TargetFacades);
	if (!Context->BlendingFactories.IsEmpty())
	{
		Context->TargetsPreloader->ForEach(
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
		Context->TargetsPreloader->OnCompleteCallback = [Settings, Context]()
		{
			if (Context->Sorter && !Context->Sorter->Init(Context, Context->TargetFacades))
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleInsidePath::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleInsidePath::FProcessor>>& NewBatch)
				{
					if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
				}))
			{
				Context->CancelExecution(TEXT("Could not find any paths to split."));
			}
		};

		Context->TargetsPreloader->StartLoading(Context->GetAsyncManager());
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

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		Path = PCGExPaths::MakePolyPath(PointDataFacade->GetIn(), 1);

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		DistanceDetails = Context->DistanceDetails;
		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		if (Settings->ProcessInputs != EPCGExPathSamplingIncludeMode::All)
		{
			bOnlySignIfClosed = Settings->bOnlySignIfClosed;
			bOnlyIncrementInsideNumIfClosed = Settings->bOnlyIncrementInsideNumIfClosed;
		}
		else
		{
			bOnlySignIfClosed = false;
			bOnlyIncrementInsideNumIfClosed = false;
		}

		if (!Context->BlendingFactories.IsEmpty())
		{
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, DistanceDetails);
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetFacades)) { return false; }
			DataBlender = UnionBlendOpsManager;
		}

		if (!DataBlender)
		{
			TSharedPtr<PCGExDataBlending::FDummyUnionBlender> DummyUnionBlender = MakeShared<PCGExDataBlending::FDummyUnionBlender>();
			DummyUnionBlender->Init(PointDataFacade, Context->TargetFacades);
			DataBlender = DummyUnionBlender;
		}

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_InsidePath(PCGEX_OUTPUT_INIT)
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
		Union->IOSet.Reserve(Context->TargetFacades.Num());

		Union->Reset();

		int32 NumInside = 0;
		int32 NumSampled = 0;

		if (RangeMax == 0) { Union->Elements.Reserve(Context->NumMaxTargets); }

		PCGExData::FConstPoint Point = PointDataFacade->GetInPoint(Index);
		const FTransform& Transform = InTransforms[Index];
		FVector Origin = Transform.GetLocation();

		PCGExData::FElement SinglePick(-1, -1);
		double WeightedDistance = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? MAX_dbl : MIN_dbl;

		double WeightedTime = 0;
		double WeightedSegmentTime = 0;

		auto SampleTarget = [&](const int32 PointIndex, const TSharedPtr<PCGExData::FFacade>& InTargetFacade)
		{
			const PCGExData::FElement TargetElement(PointIndex, InTargetFacade->Idx);

			const bool bIsInside = Path->IsInsideProjection(Transform);

			if (Settings->bOnlySampleWhenInside && !bIsInside) { return; }

			int32 NumInsideIncrement = 0;
			if (bIsInside) { if (!bOnlyIncrementInsideNumIfClosed || Path->IsClosedLoop()) { NumInsideIncrement = 1; } }

			const FVector SampleLocation = InTargetFacade->GetIn()->GetConstTransformValueRange()[PointIndex].GetLocation();

			float Alpha = 0;
			const int32 EdgeIndex = Path->GetClosestEdge(SampleLocation, Alpha);

			const FVector ModifiedOrigin = DistanceDetails->GetSourceCenter(Point, Origin, SampleLocation);
			const double DistSquared = FVector::DistSquared(ModifiedOrigin, SampleLocation);

			if (RangeMax > 0 && (DistSquared < RangeMin || DistSquared > RangeMax))
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
					if (SinglePick.Index != -1) { bReplaceWithCurrent = Context->Sorter->Sort(TargetElement, SinglePick); }
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
					SinglePick = TargetElement;
					WeightedDistance = DistSquared;

					// TODO : Adjust dist based on edge lerp
					Union->Reset();
					Union->AddWeighted_Unsafe(TargetElement, DistSquared);

					NumInside = NumInsideIncrement;

					WeightedTime = Time;
					WeightedSegmentTime = Alpha;
				}
			}
			else
			{
				// TODO : Adjust dist based on edge lerp
				WeightedDistance += DistSquared;
				Union->AddWeighted_Unsafe(TargetElement, DistSquared);

				WeightedTime += Time;
				WeightedSegmentTime += Alpha;

				NumInside += NumInsideIncrement;
			}
		};

		Context->TargetsOctree->FindElementsWithBoundsTest(
			SampleBox, [&](const PCGEx::FIndexedItem& Item)
			{
				const PCGPointOctree::FPointOctree* Octree = Context->TargetOctrees[Item.Index];
				const TSharedPtr<PCGExData::FFacade> TargetFacade = Context->TargetFacades[Item.Index];

				// Check all points within the box
				Octree->FindElementsWithBoundsTest(
					SampleBox, [&](const PCGPointOctree::FPointRef& PointRef)
					{
						SampleTarget(PointRef.Index, TargetFacade);
					});
			});

		if (Union->IsEmpty())
		{
			SamplingFailed(Index);
			return;
		}

		if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0) { Union->WeightRange = RangeMax; }
		DataBlender->ComputeWeights(Index, Union, OutWeightedPoints);

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);

		FVector WeightedSignAxis = FVector::ZeroVector;

		const double NumSampledEdges = (static_cast<double>(Union->Num()) * 0.5);
		WeightedDistance /= NumSampledEdges; // We have two points per samples
		WeightedTime /= NumSampledEdges;
		WeightedSegmentTime /= NumSampledEdges;

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

			const FTransform& TargetTransform = Context->TargetFacades[P.IO]->GetIn()->GetTransform(P.Index);
			const FQuat TargetRotation = TargetTransform.GetRotation();

			WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, TargetTransform, W);

			WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * W;

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

		const FVector CWDistance = Origin - WeightedTransform.GetLocation();
		FVector LookAt = CWDistance.GetSafeNormal();

		SamplingMask[Index] = !Union->IsEmpty();
		PCGEX_OUTPUT_VALUE(Success, Index, !Union->IsEmpty())
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, (!bOnlySignIfClosed || Path->IsClosedLoop()) ? FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance : WeightedDistance * Settings->SignedDistanceScale)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
		PCGEX_OUTPUT_VALUE(NumInside, Index, NumInside)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, NumSampled)

		bAnySuccess = true;
	}

	void FProcessor::SamplingFailed(const int32 Index)
	{
		SamplingMask[Index] = false;

		const double FailSafeDist = RangeMax;
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist * Settings->SignedDistanceScale)
		PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, FVector(FailSafeDist))
		PCGEX_OUTPUT_VALUE(NumInside, Index, -1)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
	}

	void FProcessor::CompleteWork()
	{
		for (const TSharedPtr<PCGExData::IBuffer>& Buffer : PointDataFacade->Buffers) { if (Buffer->IsWritable()) { Buffer->bResetWithFirstValue = true; } }

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
		TPointsProcessor<FPCGExSampleInsidePathContext, UPCGExSampleInsidePathSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
