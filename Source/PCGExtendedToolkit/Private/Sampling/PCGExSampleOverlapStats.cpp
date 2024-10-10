// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleOverlapStats.h"


#define LOCTEXT_NAMESPACE "PCGExSampleOverlapStatsElement"
#define PCGEX_NAMESPACE SampleOverlapStats

PCGExData::EInit UPCGExSampleOverlapStatsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

TSharedPtr<PCGExSampleOverlapStats::FOverlap> FPCGExSampleOverlapStatsContext::RegisterOverlap(
	PCGExSampleOverlapStats::FProcessor* InManager,
	PCGExSampleOverlapStats::FProcessor* InManaged,
	const FBox& InIntersection)
{
	const uint64 HashID = PCGEx::H64U(InManager->BatchIndex, InManaged->BatchIndex);

	{
		FReadScopeLock ReadScopeLock(OverlapLock);
		if (TSharedPtr<PCGExSampleOverlapStats::FOverlap>* FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }
	}

	{
		FWriteScopeLock WriteScopeLock(OverlapLock);
		if (TSharedPtr<PCGExSampleOverlapStats::FOverlap>* FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }

		TSharedPtr<PCGExSampleOverlapStats::FOverlap> NewOverlap = MakeShared<PCGExSampleOverlapStats::FOverlap>(InManager, InManaged, InIntersection);
		OverlapMap.Add(HashID, NewOverlap);
		return NewOverlap;
	}
}

void FPCGExSampleOverlapStatsContext::BatchProcessing_WorkComplete()
{
	FPCGExPointsProcessorContext::BatchProcessing_WorkComplete();

	const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>> TypedBatch = StaticCastSharedPtr<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>>(MainBatch);
	for (const TSharedPtr<PCGExSampleOverlapStats::FProcessor>& P : TypedBatch->Processors)
	{
		if (!P->bIsProcessorValid) { continue; }
		SharedOverlapSubCountMax = FMath::Max(SharedOverlapSubCountMax, P->LocalOverlapSubCountMax);
		SharedOverlapCountMax = FMath::Max(SharedOverlapCountMax, P->LocalOverlapCountMax);
	}
}

PCGEX_INITIALIZE_ELEMENT(SampleOverlapStats)

bool FPCGExSampleOverlapStatsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }


	PCGEX_CONTEXT_AND_SETTINGS(SampleOverlapStats)

	PCGEX_FOREACH_FIELD_SAMPLEOVERLAPSTATS(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Context->MainPoints->Num() < 2)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Not enough inputs; requires at least 2 to check for overlap."));
		return false;
	}

	return true;
}

bool FPCGExSampleOverlapStatsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleOverlapStatsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleOverlapStats)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any input to check for overlaps."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleOverlapStats
{
	FOverlap::FOverlap(FProcessor* InManager, FProcessor* InManaged, const FBox& InIntersection):
		Intersection(InIntersection), Manager(InManager), Managed(InManaged)
	{
		HashID = PCGEx::H64U(InManager->BatchIndex, InManaged->BatchIndex);
	}

	FProcessor::~FProcessor()
	{
	}

	void FProcessor::RegisterOverlap(FProcessor* InManaged, const FBox& Intersection)
	{
		FWriteScopeLock WriteScopeLock(RegistrationLock);
		const TSharedPtr<FOverlap> Overlap = Context->RegisterOverlap(this, InManaged, Intersection);
		if (Overlap->Manager == this) { ManagedOverlaps.Add(Overlap.ToSharedRef()); }
		Overlaps.Add(Overlap.ToSharedRef());
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_SAMPLEOVERLAPSTATS(PCGEX_OUTPUT_INIT)
		}


		// 1 - Build bounds & octrees

		InPoints = &PointDataFacade->GetIn()->GetPoints();
		NumPoints = InPoints->Num();

		PCGEx::InitArray(LocalPointBounds, NumPoints);
		OverlapSubCount.Init(0, NumPoints);
		OverlapCount.Init(0, NumPoints);

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, BoundsPreparationTask)
		BoundsPreparationTask->OnCompleteCallback =
			[&]()
			{
				Octree = MakeUnique<TBoundsOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length());
				for (TSharedPtr<PCGExDiscardByOverlap::FPointBounds> PtBounds : LocalPointBounds)
				{
					if (!PtBounds) { continue; }
					Octree->AddElement(PtBounds.Get());
				}
			};

		BoundsPreparationTask->OnIterationRangeStartCallback =
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
				FilterScope(StartIndex, Count);
			};

		switch (Settings->BoundsSource)
		{
		default:
		case EPCGExPointBoundsSource::ScaledBounds:
			BoundsPreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				if (!PointFilterCache[Index]) { return; }

				const FPCGPoint& Point = *(InPoints->GetData() + Index);
				RegisterPointBounds(Index, MakeShared<PCGExDiscardByOverlap::FPointBounds>(Index, Point, Point.GetLocalBounds().TransformBy(Point.Transform)));
			};
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			BoundsPreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				if (!PointFilterCache[Index]) { return; }

				const FPCGPoint& Point = *(InPoints->GetData() + Index);
				RegisterPointBounds(Index, MakeShared<PCGExDiscardByOverlap::FPointBounds>(Index, Point, Point.GetLocalDensityBounds().TransformBy(Point.Transform)));
			};
			break;
		case EPCGExPointBoundsSource::Bounds:
			BoundsPreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				if (!PointFilterCache[Index]) { return; }

				const FPCGPoint& Point = *(InPoints->GetData() + Index);
				FTransform TR = Point.Transform;
				TR.SetScale3D(FVector::OneVector); // Zero-out scale. I'm not sure this mode is of any use.
				RegisterPointBounds(Index, MakeShared<PCGExDiscardByOverlap::FPointBounds>(Index, Point, Point.GetLocalBounds().TransformBy(TR)));
			};
			break;
		}

		BoundsPreparationTask->StartIterations(NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);

		return true;
	}

	void FProcessor::ResolveOverlap(const int32 Index)
	{
		// For each managed overlap, find per-point intersections

		const TSharedPtr<FOverlap> ManagedOverlap = ManagedOverlaps[Index];
		const TSharedRef<FProcessor> OtherProcessor = StaticCastSharedRef<FProcessor>(*ParentBatch.Pin()->SubProcessorMap->Find(&ManagedOverlap->GetOther(this)->PointDataFacade->Source.Get()));

		Octree->FindElementsWithBoundsTest(
			FBoxCenterAndExtent(ManagedOverlap->Intersection.GetCenter(), ManagedOverlap->Intersection.GetExtent()),
			[&](const PCGExDiscardByOverlap::FPointBounds* OwnedPoint)
			{
				const FBox RefBox = OwnedPoint->Bounds.GetBox();
				const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(OwnedPoint->Bounds.GetBox());

				int32 Count = 0;

				OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
					BCAE, [&](const PCGExDiscardByOverlap::FPointBounds* OtherPoint)
					{
						const FBox Intersection = RefBox.Overlap(OtherPoint->Bounds.GetBox());

						if (!Intersection.IsValid) { return; }

						const double OverlapSize = Intersection.GetExtent().Length();
						if (Settings->ThresholdMeasure == EPCGExMeanMeasure::Relative)
						{
							if ((OverlapSize / ((OwnedPoint->Bounds.SphereRadius + OtherPoint->Bounds.SphereRadius) * 0.5)) < Settings->MinThreshold) { return; }
						}
						else
						{
							if (OverlapSize < Settings->MinThreshold) { return; }
						}

						Count++;

						ManagedOverlap->Stats.OverlapCount++;
						ManagedOverlap->Stats.OverlapVolume += Intersection.GetVolume();
					});

				if (Count > 0)
				{
					FPlatformAtomics::InterlockedExchange(&bAnyOverlap, 1);
					FPlatformAtomics::InterlockedAdd(&OverlapSubCount[OwnedPoint->Index], Count);
					FPlatformAtomics::InterlockedAdd(&OverlapCount[OwnedPoint->Index], 1);
				}
			});
	}

	void FProcessor::WriteSingleData(const int32 Index)
	{
		const int32 TOC = OverlapSubCount[Index];
		const int32 UOC = OverlapCount[Index];

		PCGEX_OUTPUT_VALUE(OverlapSubCount, Index, TOC)
		PCGEX_OUTPUT_VALUE(OverlapCount, Index, UOC)
		PCGEX_OUTPUT_VALUE(RelativeOverlapSubCount, Index, static_cast<double>(TOC) / Context->SharedOverlapSubCountMax)
		PCGEX_OUTPUT_VALUE(RelativeOverlapCount, Index, static_cast<double>(UOC) / Context->SharedOverlapCountMax)
	}

	void FProcessor::CompleteWork()
	{
		// 2 - Find overlaps between large bounds, we'll be searching only there.

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PreparationTask)
		PreparationTask->OnCompleteCallback =
			[&]()
			{
				auto WrapUp = [&]()
				{
					for (int i = 0; i < NumPoints; i++)
					{
						LocalOverlapSubCountMax = FMath::Max(LocalOverlapSubCountMax, OverlapSubCount[i]);
						LocalOverlapCountMax = FMath::Max(LocalOverlapCountMax, OverlapCount[i]);
					}
				};

				if (ManagedOverlaps.IsEmpty())
				{
					WrapUp();
					return;
				}

				PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SearchTask)
				SearchTask->OnCompleteCallback = WrapUp;
				SearchTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { ResolveOverlap(Index); };
				SearchTask->StartIterations(ManagedOverlaps.Num(), 8);
			};

		PreparationTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx)
		{
			const TSharedPtr<PCGExPointsMT::FPointsProcessorBatchBase> Parent = ParentBatch.Pin();
			const TSharedPtr<PCGExData::FFacade> OtherFacade = Parent->ProcessorFacades[Index];
			if (PointDataFacade == OtherFacade) { return; } // Skip self

			const TSharedRef<FProcessor> OtherProcessor = StaticCastSharedRef<FProcessor>(*Parent->SubProcessorMap->Find(&OtherFacade->Source.Get()));

			const FBox Intersection = Bounds.Overlap(OtherProcessor->GetBounds());
			if (!Intersection.IsValid) { return; } // No overlap

			RegisterOverlap(&OtherProcessor.Get(), Intersection);
		};
		PreparationTask->StartIterations(ParentBatch.Pin()->ProcessorFacades.Num(), 64);
	}

	void FProcessor::Write()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, SearchTask)
		SearchTask->OnCompleteCallback =
			[&]()
			{
				PointDataFacade->Write(AsyncManager);

				if (Settings->bTagIfHasAnyOverlap && bAnyOverlap) { PointDataFacade->Source->Tags->Add(Settings->HasAnyOverlapTag); }
				if (Settings->bTagIfHasNoOverlap && !bAnyOverlap) { PointDataFacade->Source->Tags->Add(Settings->HasNoOverlapTag); }
			};

		SearchTask->OnIterationCallback = [&](const int32 Index, const int32 Count, const int32 LoopIdx) { WriteSingleData(Index); };
		SearchTask->StartIterations(NumPoints, ParentBatch.Pin()->ProcessorFacades.Num());
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
