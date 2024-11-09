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
		CustomTagScore = MIN_dbl;
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

PCGExData::EIOInit UPCGExDiscardByOverlapSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::NoOutput; }

TSharedPtr<PCGExDiscardByOverlap::FOverlap> FPCGExDiscardByOverlapContext::RegisterOverlap(
	PCGExDiscardByOverlap::FProcessor* InA,
	PCGExDiscardByOverlap::FProcessor* InB,
	const FBox& InIntersection)
{
	const uint64 HashID = PCGEx::H64U(InA->BatchIndex, InB->BatchIndex);

	{
		FReadScopeLock ReadScopeLock(OverlapLock);
		if (TSharedPtr<PCGExDiscardByOverlap::FOverlap>* FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }
	}

	{
		FWriteScopeLock WriteScopeLock(OverlapLock);
		if (TSharedPtr<PCGExDiscardByOverlap::FOverlap>* FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }

		TSharedPtr<PCGExDiscardByOverlap::FOverlap> NewOverlap = MakeShared<PCGExDiscardByOverlap::FOverlap>(InA, InB, InIntersection);
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

	for (const TPair<PCGExData::FPointIO*, TSharedPtr<PCGExPointsMT::FPointsProcessor>> Pair : SubProcessorMap)
	{
		PCGExDiscardByOverlap::FProcessor* P = static_cast<PCGExDiscardByOverlap::FProcessor*>(Pair.Value.Get());
		if (!P->bIsProcessorValid) { continue; }

		if (P->HasOverlaps())
		{
			Remaining.Add(P);
			continue;
		}

		P->PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
	}

	UpdateMaxScores(Remaining);

	while (!Remaining.IsEmpty())
	{
		// Sort remaining overlaps...

		switch (Settings->Logic)
		{
		default: case EPCGExOverlapPruningLogic::LowFirst:
			Remaining.Sort(
				[](const PCGExDiscardByOverlap::FProcessor& A, const PCGExDiscardByOverlap::FProcessor& B)
				{
					return A.Weight == B.Weight ? A.PointDataFacade->Source->IOIndex > B.PointDataFacade->Source->IOIndex : A.Weight > B.Weight;
				});
			break;
		case EPCGExOverlapPruningLogic::HighFirst:
			Remaining.Sort(
				[](const PCGExDiscardByOverlap::FProcessor& A, const PCGExDiscardByOverlap::FProcessor& B)
				{
					return A.Weight == B.Weight ? A.PointDataFacade->Source->IOIndex > B.PointDataFacade->Source->IOIndex : A.Weight < B.Weight;
				});
			break;
		}

		PCGExDiscardByOverlap::FProcessor* Candidate = Remaining.Pop();

		if (Candidate->HasOverlaps()) { Candidate->Prune(Remaining); }
		else { Candidate->PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward); }

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

	return true;
}

bool FPCGExDiscardByOverlapElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardByOverlapElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DiscardByOverlap)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExDiscardByOverlap::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExDiscardByOverlap::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true; // Not really but we need the step
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any input to check for overlaps."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Processing)

	if (Context->IsState(PCGEx::State_Processing))
	{
		Context->SetAsyncState(PCGEx::State_Completing);
		Context->GetAsyncManager()->Start<PCGExDiscardByOverlap::FPruneTask>(-1, nullptr);
		return false;
	}

	if (Context->IsState(PCGEx::State_Completing))
	{
		PCGEX_ASYNC_WAIT
		Context->Done();
	}

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExDiscardByOverlap
{
	FOverlap::FOverlap(FProcessor* InManager, FProcessor* InManaged, const FBox& InIntersection):
		Intersection(InIntersection), Manager(InManager), Managed(InManaged)
	{
		HashID = PCGEx::H64U(InManager->BatchIndex, InManaged->BatchIndex);
	}

	void FProcessor::RegisterOverlap(FProcessor* InOtherProcessor, const FBox& Intersection)
	{
		FWriteScopeLock WriteScopeLock(RegistrationLock);
		const TSharedPtr<FOverlap> Overlap = Context->RegisterOverlap(this, InOtherProcessor, Intersection);
		if (Overlap->Manager == this) { ManagedOverlaps.Add(Overlap); }
		Overlaps.Add(Overlap);
	}

	void FProcessor::RemoveOverlap(const TSharedPtr<FOverlap>& InOverlap, TArray<FProcessor*>& Stack)
	{
		Overlaps.Remove(InOverlap);

		if (Overlaps.IsEmpty())
		{
			// Remove from stack & output.
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::Forward);
			Stack.Remove(this);
			return;
		}

		Stats.Remove(InOverlap->Stats, NumPoints, TotalVolume);
		UpdateWeightValues();
	}

	void FProcessor::Prune(TArray<FProcessor*>& Stack)
	{
		for (TSharedPtr<FOverlap> Overlap : Overlaps)
		{
			Overlap->GetOther(this)->RemoveOverlap(Overlap, Stack);
		}
		Overlaps.Empty();
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		// 1 - Build bounds & octrees

		InPoints = &PointDataFacade->GetIn()->GetPoints();

		NumPoints = InPoints->Num();

		PCGEx::InitArray(LocalPointBounds, NumPoints);

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, BoundsPreparationTask)

		// TODO : Optimisation for huge data set would be to first compute rough overlap
		// and then only add points within the overlap to the octree, as opposed to every single point.
		BoundsPreparationTask->OnCompleteCallback =
			[&]()
			{
				Octree = MakeUnique<FPointBoundsOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length());
				for (const TSharedPtr<FPointBounds>& PtBounds : LocalPointBounds)
				{
					if (!PtBounds) { continue; }
					Octree->AddElement(PtBounds.Get());
					TotalDensity += PtBounds->Point->Density;
				}

				VolumeDensity = NumPoints / TotalVolume;
			};

		BoundsPreparationTask->OnSubLoopStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
				FilterScope(StartIndex, Count);
			};

		if (Settings->BoundsSource == EPCGExPointBoundsSource::ScaledBounds)
		{
			BoundsPreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FPCGPoint* Point = InPoints->GetData() + Index;
				RegisterPointBounds(
					Index, MakeShared<FPointBounds>(
						Index, Point,
						PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point).ExpandBy(Settings->Expansion)));
			};
		}
		else if (Settings->BoundsSource == EPCGExPointBoundsSource::DensityBounds)
		{
			BoundsPreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FPCGPoint* Point = InPoints->GetData() + Index;
				RegisterPointBounds(
					Index, MakeShared<FPointBounds>(
						Index, Point,
						PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::DensityBounds>(Point).ExpandBy(Settings->Expansion)));
			};
		}
		else
		{
			BoundsPreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const FPCGPoint* Point = InPoints->GetData() + Index;
				RegisterPointBounds(
					Index, MakeShared<FPointBounds>(
						Index, Point,
						PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::Bounds>(Point).ExpandBy(Settings->Expansion)));
			};
		}

		BoundsPreparationTask->StartIterations(NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		// For each managed overlap, find per-point intersections

		const TSharedPtr<FOverlap> ManagedOverlap = ManagedOverlaps[Iteration];
		const TSharedRef<FProcessor> OtherProcessor = StaticCastSharedRef<FProcessor>(*ParentBatch.Pin()->SubProcessorMap->Find(&ManagedOverlap->GetOther(this)->PointDataFacade->Source.Get()));

		if (Settings->TestMode != EPCGExOverlapTestMode::Sphere)
		{
			Octree->FindElementsWithBoundsTest(
				FBoxCenterAndExtent(ManagedOverlap->Intersection.GetCenter(), ManagedOverlap->Intersection.GetExtent()),
				[&](const FPointBounds* OwnedPoint)
				{
					const double Length = OwnedPoint->LocalBounds.GetExtent().Length() * 2;
					const FMatrix InvMatrix = OwnedPoint->Point->Transform.ToMatrixNoScale().Inverse();

					OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(OwnedPoint->BSB.GetBox()), [&](const FPointBounds* OtherPoint)
						{
							const FBox Intersection = OwnedPoint->LocalBounds.Overlap(OtherPoint->TransposedBounds(InvMatrix));

							if (!Intersection.IsValid) { return; }

							const double OverlapSize = Intersection.GetExtent().Length() * 2;
							if (Settings->ThresholdMeasure == EPCGExMeanMeasure::Relative)
							{
								if ((OverlapSize / Length) < Settings->MinThreshold) { return; }
							}
							else
							{
								if (OverlapSize < Settings->MinThreshold) { return; }
							}

							ManagedOverlap->Stats.OverlapCount++;
							ManagedOverlap->Stats.OverlapVolume += Intersection.GetVolume();
						});
				});
		}
		else
		{
			Octree->FindElementsWithBoundsTest(
				FBoxCenterAndExtent(ManagedOverlap->Intersection.GetCenter(), ManagedOverlap->Intersection.GetExtent()),
				[&](const FPointBounds* OwnedPoint)
				{
					const FSphere S1 = OwnedPoint->BSB.GetSphere();

					OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
						FBoxCenterAndExtent(OwnedPoint->BSB.GetBox()), [&](const FPointBounds* OtherPoint)
						{
							double Overlap = 0;
							if (!PCGExMath::SphereOverlap(S1, OtherPoint->BSB.GetSphere(), Overlap)) { return; }

							if (Settings->ThresholdMeasure == EPCGExMeanMeasure::Relative)
							{
								if ((Overlap / S1.W) < Settings->MinThreshold) { return; }
							}
							else
							{
								if (Overlap < Settings->MinThreshold) { return; }
							}

							ManagedOverlap->Stats.OverlapCount++;
							ManagedOverlap->Stats.OverlapVolume += Overlap;
						});
				});
		}
	}

	void FProcessor::CompleteWork()
	{
		// 2 - Find overlaps between large bounds, we'll be searching only there.

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PreparationTask)
		PreparationTask->OnCompleteCallback =
			[WeakThis = TWeakPtr<FProcessor>(SharedThis(this))]()
			{
				TSharedPtr<FProcessor> This = WeakThis.Pin();
				if (!This) { return; }

				if (This->Settings->TestMode == EPCGExOverlapTestMode::Fast)
				{
					for (const TSharedPtr<FOverlap>& Overlap : This->Overlaps)
					{
						Overlap->Stats.OverlapCount = 1;
						Overlap->Stats.OverlapVolume = Overlap->Intersection.GetVolume();
					}
				}
				else
				{
					// Require one more expensive step...
					This->StartParallelLoopForRange(This->ManagedOverlaps.Num(), 8);
				}
			};
		PreparationTask->OnIterationCallback =
			[WeakThis = TWeakPtr<FProcessor>(SharedThis(this))]
			(const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				TSharedPtr<FProcessor> This = WeakThis.Pin();
				if (!This) { return; }

				const TSharedPtr<PCGExPointsMT::FPointsProcessorBatchBase> Parent = This->ParentBatch.Pin();
				const TSharedPtr<PCGExData::FFacade> OtherFacade = Parent->ProcessorFacades[Index];
				if (This->PointDataFacade == OtherFacade) { return; } // Skip self

				const TSharedRef<FProcessor> OtherProcessor = StaticCastSharedRef<FProcessor>(*Parent->SubProcessorMap->Find(&OtherFacade->Source.Get()));

				const FBox Intersection = This->Bounds.Overlap(OtherProcessor->GetBounds());
				if (!Intersection.IsValid) { return; } // No overlap

				This->RegisterOverlap(&OtherProcessor.Get(), Intersection);
			};

		PreparationTask->StartIterations(ParentBatch.Pin()->ProcessorFacades.Num(), 64);
	}

	void FProcessor::Write()
	{
		ManagedOverlaps.Empty();

		// Sanitize stats & overlaps
		for (int i = 0; i < Overlaps.Num(); i++)
		{
			const TSharedPtr<FOverlap> Overlap = Overlaps[i];

			if (Overlap->Stats.OverlapCount != 0)
			{
				Stats.Add(Overlap->Stats);
				continue;
			}

			Overlaps.RemoveAt(i);
			i--;
		}

		// Sort overlaps so we can process them

		Stats.UpdateRelative(NumPoints, TotalVolume);

		RawScores.NumPoints = static_cast<double>(NumPoints);
		RawScores.Volume = TotalVolume;
		RawScores.VolumeDensity = VolumeDensity;

		RawScores.CustomTagScore = 0;
		for (const TPair<FString, double>& TagScore : Settings->Weighting.TagScores)
		{
			if (PointDataFacade->Source->Tags->IsTagged(TagScore.Key)) { RawScores.CustomTagScore += TagScore.Value; }
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
		const FPCGExOverlapScoresWeighting& W = Context->Weights;

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
