// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPath.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestPathElement"
#define PCGEX_NAMESPACE SampleNearestPolyLine

namespace PCGExPolyLine
{
	void FSamplesStats::Update(const FSample& Infos, bool& IsNewClosest, bool& IsNewFarthest)
	{
		UpdateCount++;

		if (Infos.Distance < SampledRangeMin)
		{
			Closest = Infos;
			SampledRangeMin = Infos.Distance;
			IsNewClosest = true;
		}

		if (Infos.Distance > SampledRangeMax)
		{
			Farthest = Infos;
			SampledRangeMax = Infos.Distance;
			IsNewFarthest = true;
		}

		SampledRangeWidth = SampledRangeMax - SampledRangeMin;
	}
}

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

	return PinProperties;
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

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	TSharedPtr<PCGExData::FPointIOCollection> Targets = MakeShared<PCGExData::FPointIOCollection>(
		Context, PCGExPaths::SourcePathsLabel, PCGExData::EIOInit::NoInit, true);

	if (Targets->IsEmpty())
	{
		if (!Settings->bQuietMissingInputError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (empty datasets)")); }
		return false;
	}

	Context->TargetFacades.Reserve(Targets->Pairs.Num());
	Context->Paths.Reserve(Targets->Pairs.Num());

	FBox OctreeBounds = FBox(ForceInit);

	for (const TSharedPtr<PCGExData::FPointIO>& IO : Targets->Pairs)
	{
		const bool bClosedLoop = PCGExPaths::GetClosedLoop(IO->GetIn());

		switch (Settings->SampleInputs)
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
		TSharedPtr<PCGExPaths::FPath> Path = PCGExPaths::MakePolyPath(IO->GetIn(), 1, FVector::UpVector);
		Path->IOIndex = IO->IOIndex;
		Path->Idx = Context->Paths.Num() - 1;

		Context->TargetFacades.Add(TargetFacade.ToSharedRef());
		Context->Paths.Add(Path);

		OctreeBounds += Path->Bounds;
	}

	if (Context->Paths.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No targets (no input matches criteria)"));
		return false;
	}

	Context->DistanceDetails = PCGExDetails::MakeDistances(Settings->DistanceSettings, Settings->DistanceSettings);

	Context->PathsOctree = MakeShared<PCGEx::FIndexedItemOctree>(OctreeBounds.GetCenter(), OctreeBounds.GetExtent().Length());
	for (int i = 0; i < Context->Paths.Num(); ++i) { Context->PathsOctree->AddElement(PCGEx::FIndexedItem(i, Context->Paths[i]->Bounds)); }

	PCGEX_FOREACH_FIELD_NEARESTPATH(PCGEX_OUTPUT_VALIDATE_NAME)

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
		Context->TargetsPreloader->OnCompleteCallback = [Settings, Context]()
		{
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

		Context->TargetsPreloader->StartLoading(Context->GetAsyncManager());
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

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;

		if (Context->ApplySampling.WantsApply())
		{
			AllocateFor |= EPCGPointNativeProperties::Transform;
		}

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);


		DistanceDetails = Context->DistanceDetails;
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
		TPointsProcessor<FPCGExSampleNearestPathContext, UPCGExSampleNearestPathSettings>::PrepareLoopScopesForPoints(Loops);
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

		TArray<PCGExPolyLine::FSample> Samples;
		Samples.Reserve(Context->Paths.Num());

		const TSharedPtr<PCGExSampling::FSampingUnionData> Union = MakeShared<PCGExSampling::FSampingUnionData>();

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			Union->Reset();

			int32 NumInside = 0;
			int32 NumSampled = 0;
			int32 NumInClosed = 0;

			bool bSampledClosedLoop = false;

			double RangeMin = RangeMinGetter->Read(Index);
			double RangeMax = RangeMaxGetter->Read(Index);
			if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

			double WeightedDistance = 0;

			Samples.Reset();

			PCGExPolyLine::FSamplesStats Stats;

			const FTransform& Transform = InTransforms[Index];
			FVector Origin = Transform.GetLocation();
			PCGExData::FConstPoint Point = PointDataFacade->GetInPoint(Index);

			auto ProcessTarget = [&](const int32 EdgeIndex, const double& Lerp, const TSharedPtr<PCGExPaths::FPath>& InPath)
			{
				const FVector SampleLocation = FMath::Lerp(
					InTransforms[InPath->Edges[EdgeIndex].Start].GetLocation(),
					InTransforms[InPath->Edges[EdgeIndex].End].GetLocation(),
					Lerp);

				const FVector ModifiedOrigin = DistanceDetails->GetSourceCenter(Point, Origin, SampleLocation);
				const double Dist = FVector::Dist(ModifiedOrigin, SampleLocation);

				if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

				int32 NumInsideIncrement = 0;

				if (InPath->IsInsideProjection(Transform))
				{
					if (!bOnlyIncrementInsideNumIfClosed || InPath->IsClosedLoop()) { NumInsideIncrement = 1; }
				}

				bool IsNewClosest = false;
				bool IsNewFarthest = false;

				const double Time = (static_cast<double>(EdgeIndex) + Lerp) / static_cast<double>(InPath->NumEdges);
				PCGExPolyLine::FSample Infos(InPath->Idx, EdgeIndex, Dist, Lerp, Time);

				///////

				if (bSingleSample)
				{
					Stats.Update(Infos, IsNewClosest, IsNewFarthest);

					if ((bClosestSample && !IsNewClosest) || !IsNewFarthest) { return; }

					bSampledClosedLoop = InPath->IsClosedLoop();

					NumInside = NumInsideIncrement;
					NumInClosed = NumInsideIncrement;
				}
				else
				{
					Samples.Add(Infos);
					Stats.Update(Infos, IsNewClosest, IsNewFarthest);

					if (InPath->IsClosedLoop())
					{
						bSampledClosedLoop = true;
						NumInClosed += NumInsideIncrement;
					}

					NumInside += NumInsideIncrement;
				}
			};

			// First: Sample all possible targets
			if (!Settings->bSampleSpecificAlpha)
			{
				// At closest alpha
				Context->PathsOctree->FindElementsWithBoundsTest(
					FBox(Origin - FVector(RangeMax), Origin + FVector(RangeMax)),
					[&](const PCGEx::FIndexedItem& Item)
					{
						const TSharedPtr<PCGExPaths::FPath> Path = Context->Paths[Item.Index];
						float Lerp = 0;
						const int32 EdgeIndex = Path->GetClosestEdge(Origin, Lerp);
						ProcessTarget(EdgeIndex, Lerp, Path);
					});
			}
			else
			{
				// At specific alpha
				const double InputKey = SampleAlphaGetter->Read(Index);
				Context->PathsOctree->FindElementsWithBoundsTest(
					FBox(Origin - FVector(RangeMax), Origin + FVector(RangeMax)),
					[&](const PCGEx::FIndexedItem& Item)
					{
						const TSharedPtr<PCGExPaths::FPath>& Path = Context->Paths[Item.Index];
						const double NumSegments = Path->NumEdges;
						double Time = 0;

						switch (Settings->SampleAlphaMode)
						{
						default:
						case EPCGExPathSampleAlphaMode::Alpha:
							Time = InputKey;
							break;
						case EPCGExPathSampleAlphaMode::Time:
							Time = InputKey / NumSegments;
							break;
						case EPCGExPathSampleAlphaMode::Distance:
							Time = InputKey / Path->TotalLength;
							break;
						}

						if (Settings->bWrapClosedLoopAlpha && Path->IsClosedLoop()) { Time = PCGExMath::Tile(Time, 0.0, 1.0); }

						float Lerp = 0;
						const int32 EdgeIndex = Path->GetClosestEdge(Time, Lerp);

						ProcessTarget(EdgeIndex, Lerp, Path);
					});
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
				Stats.SampledRangeMax = RangeMax;
				Stats.SampledRangeWidth = RangeMax - RangeMin;
			}

			FTransform WeightedTransform = FTransform::Identity;
			WeightedTransform.SetScale3D(FVector::ZeroVector);

			FVector WeightedUp = SafeUpVector;
			if (LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

			FVector WeightedSignAxis = FVector::ZeroVector;
			FVector WeightedAngleAxis = FVector::ZeroVector;
			double WeightedTime = 0;
			double TotalWeight = 0;

			auto ProcessTargetInfos = [&](const PCGExPolyLine::FSample& TargetInfos, const double Weight)
			{
				const TSharedPtr<PCGExPaths::FPath>& Path = Context->Paths[TargetInfos.PathIndex];
				const PCGExData::FPoint A(Path->Edges[TargetInfos.EdgeIndex].Start, Path->IOIndex);
				const PCGExData::FPoint B(Path->Edges[TargetInfos.EdgeIndex].End, Path->IOIndex);

				Union->AddWeighted_Unsafe(A, TargetInfos.Lerp * Weight);
				Union->AddWeighted_Unsafe(B, (1 - TargetInfos.Lerp) * Weight);

				TConstPCGValueRange<FTransform> PathTransforms = Context->TargetFacades[Path->Idx]->GetIn()->GetConstTransformValueRange();
				const FTransform EdgeTransform = PCGExBlend::Lerp(PathTransforms[A.Index], PathTransforms[B.Index], TargetInfos.Lerp);
				const FQuat Quat = EdgeTransform.GetRotation();

				WeightedTransform = PCGExBlend::WeightedAdd(WeightedTransform, EdgeTransform, Weight);
				if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { PCGExBlend::WeightedAdd(WeightedUp, PCGExMath::GetDirection(Quat, Settings->LookAtUpAxis), Weight); }

				WeightedSignAxis += PCGExMath::GetDirection(Quat, Settings->SignAxis) * Weight;
				WeightedAngleAxis += PCGExMath::GetDirection(Quat, Settings->AngleAxis) * Weight;
				WeightedTime += TargetInfos.Time * Weight;

				TotalWeight += Weight;
				WeightedDistance += TargetInfos.Distance;

				NumSampled++;
			};


			if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
				Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				const PCGExPolyLine::FSample& TargetInfos = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? Stats.Closest : Stats.Farthest;
				const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
				ProcessTargetInfos(TargetInfos, Weight);
			}
			else
			{
				for (PCGExPolyLine::FSample& TargetInfos : Samples)
				{
					const double Weight = Context->WeightCurve->Eval(Stats.GetRangeRatio(TargetInfos.Distance));
					if (Weight == 0) { continue; }
					ProcessTargetInfos(TargetInfos, Weight);
				}
			}

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

			WeightedDistance /= NumSampled;
			WeightedUp.Normalize();

			const FVector CWDistance = Origin - WeightedTransform.GetLocation();
			FVector LookAt = CWDistance.GetSafeNormal();

			FTransform LookAtTransform = PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign);
			if (Context->ApplySampling.WantsApply())
			{
				PCGExData::FMutablePoint MutablePoint = PointDataFacade->GetOutPoint(Index);
				Context->ApplySampling.Apply(MutablePoint, WeightedTransform, LookAtTransform);
			}

			SamplingMask[Index] = Stats.IsValid();
			PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
			PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, LookAtTransform)
			PCGEX_OUTPUT_VALUE(Distance, Index, Settings->bOutputNormalizedDistance ? WeightedDistance : WeightedDistance * Settings->DistanceScale)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, (!bOnlySignIfClosed || NumInClosed > 0) ? FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance : WeightedDistance * Settings->SignedDistanceScale)
			PCGEX_OUTPUT_VALUE(ComponentWiseDistance, Index, Settings->bAbsoluteComponentWiseDistance ? PCGExMath::Abs(CWDistance) : CWDistance)
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
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
		TPointsProcessor<FPCGExSampleNearestPathContext, UPCGExSampleNearestPathSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
