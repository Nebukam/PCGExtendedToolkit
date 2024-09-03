// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByOverlap.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByOverlapElement"
#define PCGEX_NAMESPACE DiscardByOverlap

void FPCGExOverlapScoresWeighting::Init()
{
	StaticWeightSum = FMath::Abs(NumPoints) + FMath::Abs(Volume) + FMath::Abs(VolumeDensity) + FMath::Abs(CustomTagScore);
	NumPoints /= StaticWeightSum;
	Volume /= StaticWeightSum;
	VolumeDensity /= StaticWeightSum;
	CustomTagWeight /= StaticWeightSum;

	DynamicWeightSum = FMath::Abs(OverlapCount) + FMath::Abs(OverlapSubCount) + FMath::Abs(OverlapVolume) + FMath::Abs(OverlapVolumeDensity);
	OverlapCount /= DynamicWeightSum;
	OverlapSubCount /= DynamicWeightSum;
	OverlapVolume /= DynamicWeightSum;
	OverlapVolumeDensity /= DynamicWeightSum;

	const double Balance = FMath::Abs(DynamicBalance) + FMath::Abs(StaticBalance);
	DynamicBalance /= Balance;
	StaticBalance /= Balance;
}

void FPCGExOverlapScoresWeighting::ResetMin()
{
	OverlapCount =
		OverlapSubCount =
		OverlapVolume =
		OverlapVolumeDensity =
		NumPoints =
		Volume =
		VolumeDensity =
		CustomTagScore = TNumericLimits<double>::Min();
}

void FPCGExOverlapScoresWeighting::Max(const FPCGExOverlapScoresWeighting& Other)
{
	OverlapCount = FMath::Max(OverlapCount, Other.OverlapCount);
	OverlapSubCount = FMath::Max(OverlapSubCount, Other.OverlapSubCount);
	OverlapVolume = FMath::Max(OverlapVolume, Other.OverlapVolume);
	OverlapVolumeDensity = FMath::Max(OverlapVolumeDensity, Other.OverlapVolumeDensity);
	NumPoints = FMath::Max(NumPoints, Other.NumPoints);
	Volume = FMath::Max(Volume, Other.Volume);
	VolumeDensity = FMath::Max(VolumeDensity, Other.VolumeDensity);
	CustomTagScore = FMath::Max(CustomTagScore, Other.CustomTagScore);
}

PCGExData::EInit UPCGExDiscardByOverlapSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExDiscardByOverlapSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExPointFilter::SourceFiltersLabel, "Filters used to know whether a point should be considered for overlap or not.", Normal, {})
	return PinProperties;
}

FPCGExDiscardByOverlapContext::~FPCGExDiscardByOverlapContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGExDiscardByOverlap::FOverlap* FPCGExDiscardByOverlapContext::RegisterOverlap(
	PCGExDiscardByOverlap::FProcessor* InManager,
	PCGExDiscardByOverlap::FProcessor* InManaged,
	const FBox& InIntersection)
{
	const uint64 HashID = PCGEx::H64U(InManager->BatchIndex, InManaged->BatchIndex);

	{
		FReadScopeLock ReadScopeLock(OverlapLock);
		if (PCGExDiscardByOverlap::FOverlap** FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }
	}

	{
		FWriteScopeLock WriteScopeLock(OverlapLock);
		if (PCGExDiscardByOverlap::FOverlap** FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }

		PCGExDiscardByOverlap::FOverlap* NewOverlap = new PCGExDiscardByOverlap::FOverlap(InManager, InManaged, InIntersection);
		OverlapMap.Add(HashID, NewOverlap);
		return NewOverlap;
	}
}

void FPCGExDiscardByOverlapContext::UpdateMaxScores(const TArray<PCGExDiscardByOverlap::FProcessor*>& InStack)
{
	MaxScores.ResetMin();
	for (const PCGExDiscardByOverlap::FProcessor* C : InStack) { MaxScores.Max(C->RawScores); }
}

void FPCGExDiscardByOverlapContext::Prune()
{
	PCGEX_SETTINGS_LOCAL(DiscardByOverlap)

	TArray<PCGExDiscardByOverlap::FProcessor*> Remaining;
	Remaining.Reserve(MainBatch->GetNumProcessors());

	for (const TPair<PCGExData::FPointIO*, PCGExPointsMT::FPointsProcessor*> Pair : SubProcessorMap)
	{
		PCGExDiscardByOverlap::FProcessor* P = static_cast<PCGExDiscardByOverlap::FProcessor*>(Pair.Value);
		if (!P->bIsProcessorValid) { continue; }

		if (P->HasOverlaps())
		{
			Remaining.Add(P);
			continue;
		}

		P->PointDataFacade->Source->InitializeOutput(PCGExData::EInit::Forward);
	}

	UpdateMaxScores(Remaining);

	while (!Remaining.IsEmpty())
	{
		// Sort remaining overlaps...

		switch (Settings->Logic)
		{
		default: case EPCGExOverlapPruningLogic::LowFirst:
			Remaining.Sort([](const PCGExDiscardByOverlap::FProcessor& A, const PCGExDiscardByOverlap::FProcessor& B) { return A.Weight > B.Weight; });
			break;
		case EPCGExOverlapPruningLogic::HighFirst:
			Remaining.Sort([](const PCGExDiscardByOverlap::FProcessor& A, const PCGExDiscardByOverlap::FProcessor& B) { return A.Weight < B.Weight; });
			break;
		}

		PCGExDiscardByOverlap::FProcessor* Candidate = Remaining.Pop();

		if (Candidate->HasOverlaps()) { Candidate->Prune(Remaining); }
		else { Candidate->PointDataFacade->Source->InitializeOutput(PCGExData::EInit::Forward); }

		UpdateMaxScores(Remaining);

		for (PCGExDiscardByOverlap::FProcessor* C : Remaining) { C->UpdateWeight(MaxScores); }
	}
}

PCGEX_INITIALIZE_ELEMENT(DiscardByOverlap)

bool FPCGExDiscardByOverlapElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }


	PCGEX_CONTEXT_AND_SETTINGS(DiscardByOverlap)

	Context->Weights = Settings->Weighting;

	if (Settings->TestMode == EPCGExOverlapTestMode::Fast)
	{
		Context->Weights.DynamicBalance = 0;
		Context->Weights.StaticBalance = 1;
	}

	Context->Weights.Init();

	if (Context->MainPoints->Num() < 2)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Not enough inputs; requires at least 2 to check for overlap."));
		return false;
	}

	GetInputFactories(Context, PCGExPointFilter::SourceFiltersLabel, Context->FilterFactories, PCGExFactories::PointFilters, false);

	return true;
}

bool FPCGExDiscardByOverlapElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardByOverlapElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DiscardByOverlap)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExDiscardByOverlap::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExDiscardByOverlap::FProcessor>* NewBatch)
			{
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
				NewBatch->bRequiresWriteStep = true; // Not really but we need the step
			},
			PCGExMT::State_Processing))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any input to check for overlaps."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsState(PCGExMT::State_Processing))
	{
		Context->SetAsyncState(PCGExMT::State_Completing);
		Context->GetAsyncManager()->Start<PCGExDiscardByOverlap::FPruneTask>(-1, nullptr);
		return false;
	}

	if (Context->IsState(PCGExMT::State_Completing))
	{
		PCGEX_ASYNC_WAIT
		Context->Done();
	}

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExDiscardByOverlap
{
	FOverlap::FOverlap(FProcessor* InManager, FProcessor* InManaged, const FBox& InIntersection):
		Intersection(InIntersection), Manager(InManager), Managed(InManaged)
	{
		HashID = PCGEx::H64U(InManager->BatchIndex, InManaged->BatchIndex);
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Octree)

		PCGEX_DELETE_TARRAY(LocalPointBounds)
		PCGEX_DELETE_TARRAY(ManagedOverlaps)

		Overlaps.Empty();
	}

	void FProcessor::RegisterOverlap(FProcessor* InManaged, const FBox& Intersection)
	{
		FWriteScopeLock WriteScopeLock(RegistrationLock);
		FOverlap* Overlap = LocalTypedContext->RegisterOverlap(this, InManaged, Intersection);
		if (Overlap->Manager == this) { ManagedOverlaps.Add(Overlap); }
		Overlaps.Add(Overlap);
	}

	void FProcessor::RemoveOverlap(FOverlap* InOverlap, TArray<FProcessor*>& Stack)
	{
		Overlaps.Remove(InOverlap);

		if (Overlaps.IsEmpty())
		{
			// Remove from stack & output.
			PointDataFacade->Source->InitializeOutput(PCGExData::EInit::Forward);
			Stack.Remove(this);
			return;
		}

		Stats.Remove(InOverlap->Stats, NumPoints, TotalVolume);
		UpdateWeightValues();
	}

	void FProcessor::Prune(TArray<FProcessor*>& Stack)
	{
		for (FOverlap* Overlap : Overlaps)
		{
			Overlap->GetOther(this)->RemoveOverlap(Overlap, Stack);
			PCGEX_DELETE(Overlap)
		}

		Overlaps.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(DiscardByOverlap)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		// 1 - Build bounds & octrees

		InPoints = &PointIO->GetIn()->GetPoints();

		NumPoints = InPoints->Num();

		PCGEX_SET_NUM_UNINITIALIZED(LocalPointBounds, NumPoints)

		PCGExMT::FTaskGroup* BoundsPreparationTask = AsyncManager->CreateGroup();

		// TODO : Optimisation for huge data set would be to first compute rough overlap
		// and then only add points within the overlap to the octree, as opposed to every single point.
		BoundsPreparationTask->SetOnCompleteCallback(
			[&]()
			{
				Octree = new TBoundsOctree(Bounds.GetCenter(), Bounds.GetExtent().Length());
				for (FPointBounds* PtBounds : LocalPointBounds)
				{
					if (!PtBounds) { continue; }
					Octree->AddElement(PtBounds);
					TotalDensity += PtBounds->Point->Density;
				}

				VolumeDensity = NumPoints / TotalVolume;
			});

		BoundsPreparationTask->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				FilterScope(StartIndex, Count);
			});

		switch (Settings->BoundsSource)
		{
		default:
		case EPCGExPointBoundsSource::ScaledBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					RegisterPointBounds(Index, new FPointBounds(Index, Point, Point.GetLocalBounds().TransformBy(Point.Transform)));
				}, NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					RegisterPointBounds(Index, new FPointBounds(Index, Point, Point.GetLocalDensityBounds().TransformBy(Point.Transform)));
				}, NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);
			break;
		case EPCGExPointBoundsSource::Bounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					FTransform TR = Point.Transform;
					TR.SetScale3D(FVector::OneVector); // Zero-out scale. I'm not sure this mode is of any use.
					RegisterPointBounds(Index, new FPointBounds(Index, Point, Point.GetLocalBounds().TransformBy(TR)));
				}, NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);
			break;
		}

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		// For each managed overlap, find per-point intersections

		FOverlap* ManagedOverlap = ManagedOverlaps[Iteration];
		const FProcessor* OtherProcessor = static_cast<FProcessor*>(*ParentBatch->SubProcessorMap->Find(ManagedOverlap->GetOther(this)->PointDataFacade->Source));

		Octree->FindElementsWithBoundsTest(
			FBoxCenterAndExtent(ManagedOverlap->Intersection.GetCenter(), ManagedOverlap->Intersection.GetExtent()),
			[&](const FPointBounds* OwnedPoint)
			{
				const FBox RefBox = OwnedPoint->Bounds.GetBox();
				const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(OwnedPoint->Bounds.GetBox());

				OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
					BCAE, [&](const FPointBounds* OtherPoint)
					{
						const FBox Intersection = RefBox.Overlap(OtherPoint->Bounds.GetBox());

						if (!Intersection.IsValid) { return; }

						const double OverlapSize = Intersection.GetExtent().Length();
						if (LocalSettings->ThresholdMeasure == EPCGExMeanMeasure::Relative)
						{
							if ((OverlapSize / ((OwnedPoint->Bounds.SphereRadius + OtherPoint->Bounds.SphereRadius) * 0.5)) < LocalSettings->MinThreshold) { return; }
						}
						else
						{
							if (OverlapSize < LocalSettings->MinThreshold) { return; }
						}

						ManagedOverlap->Stats.OverlapCount++;
						ManagedOverlap->Stats.OverlapVolume += Intersection.GetVolume();
					});
			});
	}

	void FProcessor::CompleteWork()
	{
		// 2 - Find overlaps between large bounds, we'll be searching only there.

		PCGExMT::FTaskGroup* PreparationTask = AsyncManagerPtr->CreateGroup();
		PreparationTask->SetOnCompleteCallback(
			[&]()
			{
				switch (LocalSettings->TestMode)
				{
				default:
				case EPCGExOverlapTestMode::Fast:
					for (FOverlap* Overlap : Overlaps)
					{
						Overlap->Stats.OverlapCount = 1;
						Overlap->Stats.OverlapVolume = Overlap->Intersection.GetVolume();
					}
					break;
				case EPCGExOverlapTestMode::Precise:
					// Require one more expensive step...
					StartParallelLoopForRange(ManagedOverlaps.Num(), 8);
					break;
				}
			});
		PreparationTask->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const PCGExData::FFacade* OtherFacade = ParentBatch->ProcessorFacades[Index];
				if (PointDataFacade == OtherFacade) { return; } // Skip self

				FProcessor* OtherProcessor = static_cast<FProcessor*>(*ParentBatch->SubProcessorMap->Find(OtherFacade->Source));

				const FBox Intersection = Bounds.Overlap(OtherProcessor->GetBounds());
				if (!Intersection.IsValid) { return; } // No overlap

				RegisterOverlap(OtherProcessor, Intersection);
			}, ParentBatch->ProcessorFacades.Num(), 64);
	}

	void FProcessor::Write()
	{
		ManagedOverlaps.Empty();

		// Sanitize stats & overlaps
		for (int i = 0; i < Overlaps.Num(); i++)
		{
			const FOverlap* Overlap = Overlaps[i];

			if (Overlap->Stats.OverlapCount != 0)
			{
				Stats.Add(Overlap->Stats);
				continue;
			}

			Overlaps.RemoveAt(i);
			if (Overlap->Manager == this)
			{
				PCGEX_DELETE(Overlap)
			}
			i--;
		}

		// Sort overlaps so we can process them

		Stats.UpdateRelative(NumPoints, TotalVolume);

		RawScores.NumPoints = static_cast<double>(NumPoints);
		RawScores.Volume = TotalVolume;
		RawScores.VolumeDensity = VolumeDensity;

		RawScores.CustomTagScore = 0;
		for (const TPair<FString, double>& TagScore : LocalSettings->Weighting.TagScores)
		{
			if (PointIO->Tags->RawTags.Contains(TagScore.Key)) { RawScores.CustomTagScore += TagScore.Value; }
		}

		UpdateWeightValues();
	}

	void FProcessor::UpdateWeightValues()
	{
		RawScores.OverlapCount = static_cast<double>(Overlaps.Num());
		RawScores.OverlapSubCount = static_cast<double>(Stats.OverlapCount);
		RawScores.OverlapVolume = Stats.OverlapVolume;
		RawScores.OverlapVolumeDensity = Stats.OverlapVolumeAvg;
	}

	void FProcessor::UpdateWeight(const FPCGExOverlapScoresWeighting& InMax)
	{
		const FPCGExOverlapScoresWeighting& W = LocalTypedContext->Weights;

		StaticWeight = 0;
		StaticWeight += (RawScores.NumPoints / InMax.NumPoints) * W.NumPoints;
		StaticWeight += (RawScores.Volume / InMax.Volume) * W.Volume;
		StaticWeight += (RawScores.VolumeDensity / InMax.VolumeDensity) * W.VolumeDensity;
		StaticWeight += (RawScores.CustomTagScore / InMax.CustomTagScore) * W.CustomTagWeight;

		DynamicWeight = 0;
		DynamicWeight += (RawScores.OverlapCount / InMax.OverlapCount) * W.OverlapCount;
		DynamicWeight += (RawScores.OverlapSubCount / InMax.OverlapSubCount) * W.OverlapSubCount;
		DynamicWeight += (RawScores.OverlapVolume / InMax.OverlapVolume) * W.OverlapVolume;
		DynamicWeight += (RawScores.OverlapVolumeDensity / InMax.OverlapVolumeDensity) * W.OverlapVolumeDensity;

		Weight = StaticWeight * W.StaticBalance + DynamicWeight * W.DynamicBalance;

		/*
		UE_LOG(
			LogTemp, Warning, TEXT("Set #%d | W = %f | SW = %f | DW = %f | NumPoints = %f, Volume = %f, VolumeDensity = %f, OverlapCount = %f, OverlapSubCount = %f, OverlapVolume = %f, OverlapVolumeDensity = %f"),
			BatchIndex,
			Weight,
			StaticWeight,
			DynamicWeight,
			RawScores.NumPoints,
			RawScores.Volume,
			RawScores.VolumeDensity,
			RawScores.OverlapCount,
			RawScores.OverlapSubCount,
			RawScores.OverlapVolume,
			RawScores.OverlapVolumeDensity)
			*/
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
