// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExExtrudeTensors.h"

#include "Containers/PCGExScopedContainers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Core/PCGExPointFilter.h"
#include "Core/PCGExTensorFactoryProvider.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Sorting/PCGExPointSorter.h"
#include "Sorting/PCGExSortingDetails.h"

#define LOCTEXT_NAMESPACE "PCGExExtrudeTensorsElement"
#define PCGEX_NAMESPACE ExtrudeTensors

PCGEX_SETTING_VALUE_IMPL(UPCGExExtrudeTensorsSettings, MaxLength, double, MaxLengthInput, MaxLengthAttribute, MaxLength)
PCGEX_SETTING_VALUE_IMPL(UPCGExExtrudeTensorsSettings, MaxPointsCount, int32, MaxPointsCountInput, MaxPointsCountAttribute, MaxPointsCount)
PCGEX_SETTING_VALUE_IMPL_BOOL(UPCGExExtrudeTensorsSettings, Iterations, int32, bUsePerPointMaxIterations, IterationsAttribute, Iterations)

TArray<FPCGPinProperties> UPCGExExtrudeTensorsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExTensor::SourceTensorsLabel, "Tensors", Required, FPCGExDataTypeInfoTensor::AsId())
	PCGEX_PIN_FILTERS(PCGExFilters::Labels::SourceStopConditionLabel, "Extruded points will be tested against those filters. If a filter returns true, the extrusion point is considered 'out-of-bounds'.", Normal)

	if (bDoExternalPathIntersections) { PCGEX_PIN_POINTS(PCGExPaths::Labels::SourcePathsLabel, "Paths that will be checked for intersections while extruding.", Normal) }
	else { PCGEX_PIN_POINTS(PCGExPaths::Labels::SourcePathsLabel, "(This is only there to preserve connections, enable it in the settings.)", Advanced) }

	PCGExSorting::DeclareSortingRulesInputs(PinProperties, bDoSelfPathIntersections ? EPCGPinStatus::Normal : EPCGPinStatus::Advanced);

	return PinProperties;
}

bool UPCGExExtrudeTensorsSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	OutRules.Append(PCGExSorting::GetSortingRules(InContext, PCGExSorting::Labels::SourceSortingRules));
	return !OutRules.IsEmpty();
}

PCGEX_INITIALIZE_ELEMENT(ExtrudeTensors)
PCGEX_ELEMENT_BATCH_POINT_IMPL_ADV(ExtrudeTensors)

FName UPCGExExtrudeTensorsSettings::GetMainInputPin() const { return PCGExCommon::Labels::SourceSeedsLabel; }
FName UPCGExExtrudeTensorsSettings::GetMainOutputPin() const { return PCGExPaths::Labels::OutputPathsLabel; }

bool FPCGExExtrudeTensorsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ExtrudeTensors)

	PCGEX_FWD(ExternalPathIntersections)
	Context->ExternalPathIntersections.Init();

	PCGEX_FWD(SelfPathIntersections)
	Context->SelfPathIntersections.Init();

	PCGEX_FWD(MergeDetails)
	Context->MergeDetails.Init();

	if (!PCGExFactories::GetInputFactories(InContext, PCGExTensor::SourceTensorsLabel, Context->TensorFactories, {PCGExFactories::EType::Tensor}))
	{
		return false;
	}

	GetInputFactories(Context, PCGExFilters::Labels::SourceStopConditionLabel, Context->StopFilterFactories, PCGExFactories::PointFilters, false);

	PCGExPointFilter::PruneForDirectEvaluation(Context, Context->StopFilterFactories);

	if (Context->TensorFactories.IsEmpty())
	{
		PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("Missing tensors."))
		return false;
	}

	Context->ClosedLoopSquaredDistance = FMath::Square(Settings->ClosedLoopSearchDistance);
	Context->ClosedLoopSearchDot = PCGExMath::DegreesToDot(Settings->ClosedLoopSearchAngle);

	return true;
}

bool FPCGExExtrudeTensorsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExExtrudeTensorsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ExtrudeTensors)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->AddConsumableAttributeName(Settings->IterationsAttribute);

		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
			NewBatch->bPrefetchData = true;
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to subdivide."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExExtrudeTensors
{
	FExtrusion::FExtrusion(const int32 InSeedIndex, const TSharedRef<PCGExData::FFacade>& InFacade, const int32 InMaxIterations)
		: SeedIndex(InSeedIndex), RemainingIterations(InMaxIterations), PointDataFacade(InFacade)
	{
		ExtrudedPoints.Reserve(InMaxIterations);
		Origin = InFacade->Source->GetInPoint(SeedIndex);
		ProxyHead = PCGExData::FProxyPoint(Origin);
		ExtrudedPoints.Emplace(Origin.GetTransform());
		SetHead(ExtrudedPoints.Last());
	}

	PCGExMath::FSegment FExtrusion::GetHeadSegment() const
	{
		return PCGExMath::FSegment(ExtrudedPoints.Last(1).GetLocation(), ExtrudedPoints.Last().GetLocation(), Context->ExternalPathIntersections.Tolerance);
	}

	void FExtrusion::SetHead(const FTransform& InHead)
	{
		LastInsertion = InHead.GetLocation();

		Head = InHead;
		ProxyHead.Transform = InHead;

		ExtrudedPoints.Last() = Head;
		Metrics = PCGExPaths::FPathMetrics(LastInsertion);

		const double Tol = Context ? Context->SelfPathIntersections.Tolerance : 0;
		PCGEX_SET_BOX_TOLERANCE(Bounds, (Metrics.Last + FVector::OneVector * 1), (Metrics.Last + FVector::OneVector * -1), Tol);

		///
		///
	}

	void FExtrusion::Complete()
	{
		if (bIsComplete || bIsStopped) { return; }

		bIsComplete = true;

		// TODO : Turn Extruded points into data
		// - Only allocate transform
		// - Set single value for all other ranges

		bIsValidPath = Settings->PathOutputDetails.Validate(ExtrudedPoints.Num());

		if (!bIsValidPath)
		{
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::NoInit);
			PointDataFacade->Source->Disable();
			return;
		}

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
		PCGExPaths::Helpers::SetClosedLoop(OutPointData, false);

		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPointData, ExtrudedPoints.Num(), PointDataFacade->GetAllocations());

		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();
		for (int i = 0; i < ExtrudedPoints.Num(); i++) { OutTransforms[i] = ExtrudedPoints[i]; }


		PCGExPaths::Helpers::SetClosedLoop(PointDataFacade->GetOut(), bIsClosedLoop);

		if (Settings->bTagIfIsStoppedByFilters && bHitStopFilters) { PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedByFiltersTag); }
		if (bHitIntersection)
		{
			// TODO : Grab data from intersection
			if (Settings->bTagIfIsStoppedByIntersection) { PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedByIntersectionTag); }
			if (Settings->bTagIfIsStoppedBySelfIntersection && bHitSelfIntersection) { PointDataFacade->Source->Tags->AddRaw(Settings->IsStoppedBySelfIntersectionTag); }
			if (Settings->bTagIfSelfMerged && bIsSelfMerged) { PointDataFacade->Source->Tags->AddRaw(Settings->IsSelfMergedTag); }
		}
		if (Settings->bTagIfChildExtrusion && bIsChildExtrusion) { PointDataFacade->Source->Tags->AddRaw(Settings->IsChildExtrusionTag); }
		if (Settings->bTagIfIsFollowUp && bIsFollowUp) { PointDataFacade->Source->Tags->AddRaw(Settings->IsFollowUpTag); }


		PointDataFacade->Source->GetOutKeys(true);
	}

	void FExtrusion::CutOff(const FVector& InCutOff)
	{
		FVector PrevPos = ExtrudedPoints.Last(1).GetLocation();

		/*
		if(PrevPos == InCutOff)
		{
			// Dedupe last point, perfect in-place cut
			ExtrudedPoints.Pop();
			SegmentBounds.Pop();
			return;
		}
		*/

		ExtrudedPoints.Last().SetLocation(InCutOff);
		bHitIntersection = true;
		bHitSelfIntersection = true;

		Complete();

		PCGEX_BOX_TOLERANCE(OEBox, PrevPos, ExtrudedPoints.Last().GetLocation(), (Context->SelfPathIntersections.ToleranceSquared + 1));
		SegmentBounds.Last() = OEBox;

		bIsStopped = true;
	}

	void FExtrusion::Shorten(const FVector& InCutOff)
	{
		const FVector A = ExtrudedPoints.Last(1).GetLocation();
		if (FVector::DistSquared(A, InCutOff) < FVector::DistSquared(A, ExtrudedPoints.Last().GetLocation()))
		{
			ExtrudedPoints.Last().SetLocation(InCutOff);
		}
	}

	PCGExMath::FClosestPosition FExtrusion::FindCrossing(const PCGExMath::FSegment& InSegment, bool& OutIsLastSegment, PCGExMath::FClosestPosition& OutClosestPosition, const int32 TruncateSearch) const
	{
		if (!Bounds.Intersect(InSegment.Bounds)) { return PCGExMath::FClosestPosition(); }

		const int32 MaxSearches = SegmentBounds.Num() - TruncateSearch;

		if (MaxSearches <= 0) { return PCGExMath::FClosestPosition(); }

		PCGExMath::FClosestPosition Crossing(InSegment.A);

		const int32 LastSegment = SegmentBounds.Num() - 1;
		const double SqrTolerance = Context->SelfPathIntersections.ToleranceSquared;
		const uint8 Strictness = Context->SelfPathIntersections.Strictness;

		for (int i = 0; i < MaxSearches; i++)
		{
			if (!SegmentBounds[i].Intersect(InSegment.Bounds)) { continue; }

			FVector A = ExtrudedPoints[i].GetLocation();
			FVector B = ExtrudedPoints[i + 1].GetLocation();

			if (Context->SelfPathIntersections.bWantsDotCheck)
			{
				if (!Context->SelfPathIntersections.CheckDot(FMath::Abs(FVector::DotProduct((B - A).GetSafeNormal(), InSegment.Direction)))) { continue; }
			}

			FVector OutSelf = FVector::ZeroVector;
			FVector OutOther = FVector::ZeroVector;

			if (!InSegment.FindIntersection(A, B, SqrTolerance, OutSelf, OutOther, Strictness))
			{
				OutClosestPosition.Update(OutOther);
				continue;
			}

			OutClosestPosition.Update(OutOther);
			Crossing.Update(OutOther, i);
		}

		OutIsLastSegment = Crossing.Index == LastSegment;
		return Crossing;
	}

	bool FExtrusion::TryMerge(const PCGExMath::FSegment& InSegment, const PCGExMath::FClosestPosition& InMerge)
	{
		// Merge
		if (!Settings->bMergeOnProximity || !InMerge) { return false; }

		if (Context->MergeDetails.bWantsDotCheck)
		{
			if (!Context->MergeDetails.CheckDot(FMath::Abs(FVector::DotProduct(InMerge.Direction(), InSegment.Direction)))) { return false; }
		}

		if (InMerge.DistSquared > Context->MergeDetails.ToleranceSquared) { return false; }

		bHitIntersection = true;
		bHitSelfIntersection = true;
		bIsSelfMerged = true;

		return true;
	}

	void FExtrusion::Cleanup()
	{
		SegmentBounds.Empty();
	}

	void FExtrusion::StartNewExtrusion()
	{
		if (RemainingIterations > 1)
		{
			// We re-entered bounds from a previously completed path.
			if (const TSharedPtr<FExtrusion> ChildExtrusion = Processor->InitExtrusionFromExtrusion(SharedThis(this)))
			{
				ChildExtrusion->bIsChildExtrusion = true;
				ChildExtrusion->bIsFollowUp = bIsComplete;
			}
		}
	}

	bool FExtrusion::OnAdvanced(const bool bStop)
	{
		RemainingIterations--;

		if (bStop || RemainingIterations <= 0)
		{
			Complete();
			bIsStopped = true;
		}

		return !bIsStopped;
	}

	void FExtrusion::Insert(const FTransform& InPoint)
	{
		bAdvancedOnly = false;

		ExtrudedPoints.Emplace(InPoint);
		LastInsertion = InPoint.GetLocation();
		//if (Settings->bRefreshSeed) { NewPoint.Seed = PCGExRandomHelpers::ComputeSpatialSeed(LastInsertion, FVector(Origin.GetLocation())); }

		if (Settings->bDoSelfPathIntersections)
		{
			PCGEX_BOX_TOLERANCE(OEBox, ExtrudedPoints.Last(1).GetLocation(), ExtrudedPoints.Last().GetLocation(), (Context->SelfPathIntersections.ToleranceSquared + 1));
			SegmentBounds.Emplace(OEBox);

			Bounds += OEBox;
		}
	}

	FProcessor::~FProcessor()
	{
	}

	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TProcessor<FPCGExExtrudeTensorsContext, UPCGExExtrudeTensorsSettings>::RegisterBuffersDependencies(FacadePreloader);

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		if (Settings->GetSortingRules(ExecutionContext, RuleConfigs) && !RuleConfigs.IsEmpty())
		{
			Sorter = MakeShared<PCGExSorting::FSorter>(Context, PointDataFacade, RuleConfigs);
			Sorter->SortDirection = Settings->SortDirection;
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExExtrudeTensors::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (Sorter && !Sorter->Init(Context)) { Sorter.Reset(); }

		StaticPaths = MakeShared<TArray<TSharedPtr<PCGExPaths::FPath>>>();

		if (!Context->StopFilterFactories.IsEmpty())
		{
			StopFilters = MakeShared<PCGExPointFilter::FManager>(PointDataFacade);
			if (!StopFilters->Init(Context, Context->StopFilterFactories)) { StopFilters.Reset(); }
		}

		TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>(Settings->TensorHandlerDetails);
		if (!TensorsHandler->Init(Context, Context->TensorFactories, PointDataFacade)) { return false; }

		AttributesToPathTags = Settings->AttributesToPathTags;
		if (!AttributesToPathTags.Init(Context, PointDataFacade)) { return false; }

		PerPointIterations = Settings->GetValueSettingIterations();
		if (!PerPointIterations->Init(PointDataFacade, false, true)) { return false; }
		if (!PerPointIterations->IsConstant())
		{
			if (Settings->bUseMaxFromPoints) { RemainingIterations = FMath::Max(RemainingIterations, PerPointIterations->Max()); }
		}
		else { RemainingIterations = Settings->Iterations; }

		if (Settings->bUseMaxLength)
		{
			MaxLength = Settings->GetValueSettingMaxLength();
			if (!MaxLength->Init(PointDataFacade, false)) { return false; }
		}

		if (Settings->bUseMaxPointsCount)
		{
			MaxPointsCount = Settings->GetValueSettingMaxPointsCount();
			if (!MaxPointsCount->Init(PointDataFacade, false)) { return false; }
		}

		const int32 NumPoints = PointDataFacade->GetNum();
		PCGExArrayHelpers::InitArray(ExtrusionQueue, NumPoints);
		PointFilterCache.Init(true, NumPoints);

		Context->MainPoints->IncreaseReserve(NumPoints);

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::InitExtrusionFromSeed(const int32 InSeedIndex)
	{
		const int32 Iterations = PerPointIterations->Read(InSeedIndex);
		if (Iterations < 1) { return; }

		bool bIsStopped = false;
		if (StopFilters)
		{
			const PCGExData::FProxyPoint ProxyPoint(PointDataFacade->Source->GetInPoint(InSeedIndex));
			bIsStopped = StopFilters->Test(ProxyPoint);
			if (Settings->bIgnoreStoppedSeeds && bIsStopped) { return; }
		}

		const TSharedPtr<FExtrusion> NewExtrusion = CreateExtrusionTemplate(InSeedIndex, Iterations);
		if (!NewExtrusion) { return; }

		NewExtrusion->bIsProbe = bIsStopped;
		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = MaxLength->Read(InSeedIndex); }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = MaxPointsCount->Read(InSeedIndex); }

		ExtrusionQueue[InSeedIndex] = NewExtrusion;
	}

	TSharedPtr<FExtrusion> FProcessor::InitExtrusionFromExtrusion(const TSharedRef<FExtrusion>& InExtrusion)
	{
		if (!Settings->bAllowChildExtrusions) { return nullptr; }

		const TSharedPtr<FExtrusion> NewExtrusion = CreateExtrusionTemplate(InExtrusion->SeedIndex, InExtrusion->RemainingIterations);
		if (!NewExtrusion) { return nullptr; }

		NewExtrusion->SetHead(InExtrusion->Head);

		{
			FWriteScopeLock WriteScopeLock(NewExtrusionLock);
			NewExtrusions.Add(NewExtrusion);
		}

		return NewExtrusion;
	}

	void FProcessor::SortQueue()
	{
		switch (Settings->SelfIntersectionMode)
		{
		case EPCGExSelfIntersectionMode::PathLength: if (Sorter)
			{
				if (Settings->SortDirection == EPCGExSortDirection::Ascending)
				{
					ExtrusionQueue.Sort([S = Sorter](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
					{
						if (EA->Metrics.Length == EB->Metrics.Length) { return S->Sort(EA->SeedIndex, EB->SeedIndex); }
						return EA->Metrics.Length > EB->Metrics.Length;
					});
				}
				else
				{
					ExtrusionQueue.Sort([S = Sorter](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
					{
						if (EA->Metrics.Length == EB->Metrics.Length) { return S->Sort(EA->SeedIndex, EB->SeedIndex); }
						return EA->Metrics.Length < EB->Metrics.Length;
					});
				}
			}
			else
			{
				ExtrusionQueue.Sort([](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
				{
					return EA->Metrics.Length > EB->Metrics.Length;
				});
			}
			break;
		case EPCGExSelfIntersectionMode::SortingOnly: if (Sorter)
			{
				ExtrusionQueue.Sort([S = Sorter](const TSharedPtr<FExtrusion>& EA, const TSharedPtr<FExtrusion>& EB)
				{
					return S->Sort(EA->SeedIndex, EB->SeedIndex);
				});
			}
			break;
		}
	}

	void FProcessor::PrepareLoopScopesForRanges(const TArray<PCGExMT::FScope>& Loops)
	{
		CompletedExtrusions = MakeShared<PCGExMT::TScopedArray<TSharedPtr<FExtrusion>>>(Loops);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::ExtrudeTensors::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		PCGEX_SCOPE_LOOP(Index) { InitExtrusionFromSeed(Index); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (UpdateExtrusionQueue()) { StartParallelLoopForRange(ExtrusionQueue.Num(), 32); }
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			if (TSharedPtr<FExtrusion> Extrusion = ExtrusionQueue[Index]; Extrusion && !Extrusion->Advance())
			{
				Extrusion->Complete();
				CompletedExtrusions->Get(Scope)->Add(Extrusion);
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		RemainingIterations--;

		if (Settings->bDoSelfPathIntersections)
		{
			const bool bMergeFirst = Settings->SelfIntersectionPriority == EPCGExSelfIntersectionPriority::Merge;
			const int32 NumQueuedExtrusions = ExtrusionQueue.Num();

			SortQueue();

			for (int i = 0; i < NumQueuedExtrusions; i++)
			{
				TSharedPtr<FExtrusion> E = ExtrusionQueue[i];

				if (E->bAdvancedOnly || !E->bIsExtruding) { continue; }

				const PCGExMath::FSegment HeadSegment = E->GetHeadSegment();
				PCGExMath::FClosestPosition Crossing(HeadSegment.A);
				PCGExMath::FClosestPosition Merge(HeadSegment.Lerp(Settings->ProximitySegmentBalance));
				PCGExMath::FClosestPosition PreMerge(Merge.Origin);

				for (int j = 0; j < ExtrusionQueue.Num(); j++)
				{
					const TSharedPtr<FExtrusion> OE = ExtrusionQueue[j];
					if (!OE->bIsExtruding) { continue; }
					if (!OE->Bounds.Intersect(HeadSegment.Bounds)) { continue; }

					const int32 TruncateSearch = i == j ? 2 : 0;
					bool bIsLastSegment = false;
					if (j > i)
					{
						if (PCGExMath::FClosestPosition LocalCrossing = OE->FindCrossing(HeadSegment, bIsLastSegment, PreMerge, TruncateSearch))
						{
							if (bIsLastSegment)
							{
								// Lower priority path
								// Cut will happen the other way around
								continue;
							}

							Merge.Update(PreMerge);
							Crossing.Update(LocalCrossing, j);
						}
					}
					else
					{
						if (PCGExMath::FClosestPosition LocalCrossing = OE->FindCrossing(HeadSegment, bIsLastSegment, PreMerge, TruncateSearch))
						{
							// Crossing found
							if (bMergeFirst) { Merge.Update(PreMerge); }
							Crossing.Update(LocalCrossing, j);
						}
						else
						{
							// Update merge instead
							Merge.Update(PreMerge);
						}
					}
				}

				if (bMergeFirst)
				{
					if (E->TryMerge(HeadSegment, Merge))
					{
						E->CutOff(Merge);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
					else if (Crossing)
					{
						E->CutOff(Crossing);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
				}
				else
				{
					if (Crossing)
					{
						E->CutOff(Crossing);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
					else if (E->TryMerge(HeadSegment, Merge))
					{
						E->CutOff(Merge);
						CompletedExtrusions->Arrays[0]->Add(E);
					}
				}
			}
		}

		if (UpdateExtrusionQueue()) { StartParallelLoopForRange(ExtrusionQueue.Num(), 32); }
	}

	bool FProcessor::UpdateExtrusionQueue()
	{
		if (RemainingIterations <= 0) { return false; }

		int32 WriteIndex = 0;
		for (int i = 0; i < ExtrusionQueue.Num(); i++)
		{
			if (TSharedPtr<FExtrusion> E = ExtrusionQueue[i]; E && !E->bIsComplete) { ExtrusionQueue[WriteIndex++] = E; }
		}

		ExtrusionQueue.SetNum(WriteIndex);

		if (!NewExtrusions.IsEmpty())
		{
			ExtrusionQueue.Reserve(ExtrusionQueue.Num() + NewExtrusions.Num());
			ExtrusionQueue.Append(NewExtrusions);
			NewExtrusions.Reset();
		}

		if (ExtrusionQueue.IsEmpty()) { return false; }

		// Convert completed paths to static collision constraints
		if (Settings->bDoSelfPathIntersections && CompletedExtrusions)
		{
			CompletedExtrusions->ForEach([&](TArray<TSharedPtr<FExtrusion>>& Completed)
			{
				StaticPaths.Get()->Reserve(StaticPaths.Get()->Num() + Completed.Num());
				for (const TSharedPtr<FExtrusion>& E : Completed)
				{
					E->Cleanup();

					if (!E->bIsValidPath) { continue; }

					TSharedPtr<PCGExPaths::FPath> StaticPath = MakeShared<PCGExPaths::FPath>(E->PointDataFacade->GetOut(), Settings->ExternalPathIntersections.Tolerance);
					StaticPath->BuildEdgeOctree();
					StaticPaths.Get()->Add(StaticPath);
				}
			});

			CompletedExtrusions.Reset();
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		for (const TSharedPtr<FExtrusion>& E : ExtrusionQueue) { if (E) { E->Complete(); } }
		CompletedExtrusions.Reset();
		ExtrusionQueue.Empty();
		StaticPaths->Empty();
	}

	TSharedPtr<FExtrusion> FProcessor::CreateExtrusionTemplate(const int32 InSeedIndex, const int32 InMaxIterations)
	{
		const TSharedPtr<PCGExData::FPointIO> NewIO = Context->MainPoints->Emplace_GetRef(PointDataFacade->Source->GetIn(), PCGExData::EIOInit::NoInit);
		if (!NewIO) { return nullptr; }

		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, NewIO.ToSharedRef());
		if (!Facade->Source->InitializeOutput(PCGExData::EIOInit::New)) { return nullptr; }

		TSharedPtr<FExtrusion> NewExtrusion = nullptr;

#define PCGEX_NEW_EXTRUSION(_FLAGS) NewExtrusion = MakeShared<TExtrusion<_FLAGS>>(InSeedIndex, Facade.ToSharedRef(), InMaxIterations);
#define PCGEX_1_FLAGS_CASE(_A) case EExtrusionFlags::_A : PCGEX_NEW_EXTRUSION(EExtrusionFlags::_A) break;
#define PCGEX_2_FLAGS(_A, _B) static_cast<EExtrusionFlags>(static_cast<uint32>(EExtrusionFlags::_A) | static_cast<uint32>(EExtrusionFlags::_B))
#define PCGEX_2_FLAGS_CASE(_A, _B) case PCGEX_2_FLAGS(_A, _B) : PCGEX_NEW_EXTRUSION(PCGEX_2_FLAGS(_A, _B)) break;
#define PCGEX_3_FLAGS(_A, _B, _C) static_cast<EExtrusionFlags>(static_cast<uint32>(EExtrusionFlags::_A) | static_cast<uint32>(EExtrusionFlags::_B) | static_cast<uint32>(EExtrusionFlags::_C))
#define PCGEX_3_FLAGS_CASE(_A, _B, _C) case PCGEX_3_FLAGS(_A, _B, _C) : PCGEX_NEW_EXTRUSION(PCGEX_3_FLAGS(_A, _B, _C)) break;
#define PCGEX_4_FLAGS(_A, _B, _C, _D) static_cast<EExtrusionFlags>(static_cast<uint32>(EExtrusionFlags::_A) | static_cast<uint32>(EExtrusionFlags::_B) | static_cast<uint32>(EExtrusionFlags::_C) | static_cast<uint32>(EExtrusionFlags::_D))
#define PCGEX_4_FLAGS_CASE(_A, _B, _C, _D) case PCGEX_4_FLAGS(_A, _B, _C, _D) : PCGEX_NEW_EXTRUSION(PCGEX_4_FLAGS(_A, _B, _C, _D)) break;

		switch (ComputeFlags())
		{
		PCGEX_1_FLAGS_CASE(None)
		PCGEX_1_FLAGS_CASE(Bounded)
		PCGEX_1_FLAGS_CASE(AllowsChildren)
		PCGEX_1_FLAGS_CASE(ClosedLoop)
		PCGEX_1_FLAGS_CASE(CollisionCheck)
		PCGEX_2_FLAGS_CASE(Bounded, AllowsChildren)
		PCGEX_2_FLAGS_CASE(Bounded, ClosedLoop)
		PCGEX_2_FLAGS_CASE(AllowsChildren, ClosedLoop)
		PCGEX_2_FLAGS_CASE(Bounded, CollisionCheck)
		PCGEX_2_FLAGS_CASE(ClosedLoop, CollisionCheck)
		PCGEX_2_FLAGS_CASE(AllowsChildren, CollisionCheck)
		PCGEX_3_FLAGS_CASE(Bounded, AllowsChildren, ClosedLoop)
		PCGEX_3_FLAGS_CASE(CollisionCheck, AllowsChildren, ClosedLoop)
		PCGEX_3_FLAGS_CASE(Bounded, CollisionCheck, ClosedLoop)
		PCGEX_3_FLAGS_CASE(Bounded, AllowsChildren, CollisionCheck)
		PCGEX_4_FLAGS_CASE(Bounded, AllowsChildren, ClosedLoop, CollisionCheck)
		default: checkNoEntry(); // You missed flags dummy
			break;
		}

#undef PCGEX_NEW_EXTRUSION
#undef PCGEX_1_FLAGS_CASE
#undef PCGEX_2_FLAGS
#undef PCGEX_2_FLAGS_CASE
#undef PCGEX_3_FLAGS
#undef PCGEX_3_FLAGS_CASE
#undef PCGEX_4_FLAGS
#undef PCGEX_4_FLAGS_CASE

		if (Settings->bUseMaxLength) { NewExtrusion->MaxLength = MaxLength->Read(InSeedIndex); }
		if (Settings->bUseMaxPointsCount) { NewExtrusion->MaxPointCount = MaxPointsCount->Read(InSeedIndex); }

		NewExtrusion->PointDataFacade->Source->IOIndex = BatchIndex * 1000000 + InSeedIndex;
		AttributesToPathTags.Tag(PointDataFacade->GetInPoint(InSeedIndex), Facade->Source);

		NewExtrusion->Processor = this;
		NewExtrusion->Context = Context;
		NewExtrusion->Settings = Settings;
		NewExtrusion->TensorsHandler = TensorsHandler;
		NewExtrusion->StopFilters = StopFilters;
		NewExtrusion->SolidPaths = StaticPaths;

		return NewExtrusion;
	}

	FBatch::FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: TBatch<FProcessor>(InContext, InPointsCollection)
	{
	}

	void FBatch::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TaskManager = InTaskManager;

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ExtrudeTensors)

		if (Settings->bDoExternalPathIntersections)
		{
			if (TryGetFacades(Context, PCGExPaths::Labels::SourcePathsLabel, Context->PathsFacades, false, true))
			{
				Context->ExternalPaths.Reserve(Context->PathsFacades.Num());
				for (const TSharedPtr<PCGExData::FFacade>& Facade : Context->PathsFacades)
				{
					TSharedPtr<PCGExPaths::FPath> Path = MakeShared<PCGExPaths::FPath>(Facade->GetIn(), Settings->ExternalPathIntersections.Tolerance);
					Context->ExternalPaths.Add(Path);
					Path->BuildEdgeOctree();
				}
			}
		}

		OnPathsPrepared();
	}

	void FBatch::OnPathsPrepared()
	{
		TBatch<FProcessor>::Process(TaskManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
