// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleOverlapStats.h"

#define LOCTEXT_NAMESPACE "PCGExSampleOverlapStatsElement"
#define PCGEX_NAMESPACE SampleOverlapStats

PCGExData::EInit UPCGExSampleOverlapStatsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExSampleOverlapStatsContext::~FPCGExSampleOverlapStatsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGExSampleOverlapStats::FOverlap* FPCGExSampleOverlapStatsContext::RegisterOverlap(
	PCGExSampleOverlapStats::FProcessor* InManager,
	PCGExSampleOverlapStats::FProcessor* InManaged,
	const FBox& InIntersection)
{
	const uint64 HashID = PCGEx::H64U(InManager->BatchIndex, InManaged->BatchIndex);

	{
		FReadScopeLock ReadScopeLock(OverlapLock);
		if (PCGExSampleOverlapStats::FOverlap** FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }
	}

	{
		FWriteScopeLock WriteScopeLock(OverlapLock);
		if (PCGExSampleOverlapStats::FOverlap** FoundPtr = OverlapMap.Find(HashID)) { return *FoundPtr; }

		PCGExSampleOverlapStats::FOverlap* NewOverlap = new PCGExSampleOverlapStats::FOverlap(InManager, InManaged, InIntersection);
		OverlapMap.Add(HashID, NewOverlap);
		return NewOverlap;
	}
}

void FPCGExSampleOverlapStatsContext::MTState_PointsCompletingWorkDone()
{
	FPCGExPointsProcessorContext::MTState_PointsCompletingWorkDone();

	PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>* TypedBatch = static_cast<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>*>(MainBatch);
	for (PCGExSampleOverlapStats::FProcessor* P : TypedBatch->Processors)
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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSampleOverlapStats::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any input to check for overlaps."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

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

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleOverlapStats)

		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_SAMPLEOVERLAPSTATS(PCGEX_OUTPUT_INIT)
		}

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		// 1 - Build bounds & octrees

		InPoints = &PointIO->GetIn()->GetPoints();
		NumPoints = InPoints->Num();

		PCGEX_SET_NUM_UNINITIALIZED(LocalPointBounds, NumPoints)
		PCGEX_SET_NUM(OverlapSubCount, NumPoints)
		PCGEX_SET_NUM(OverlapCount, NumPoints)

		PCGEX_ASYNC_GROUP(AsyncManagerPtr, BoundsPreparationTask)
		BoundsPreparationTask->SetOnCompleteCallback(
			[&]()
			{
				Octree = new TBoundsOctree(Bounds.GetCenter(), Bounds.GetExtent().Length());
				for (PCGExDiscardByOverlap::FPointBounds* PtBounds : LocalPointBounds)
				{
					if (!PtBounds) { continue; }
					Octree->AddElement(PtBounds);
				}
			});

		BoundsPreparationTask->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);
				FilterScope(StartIndex, Count);
			});

		switch (Settings->BoundsSource)
		{
		default:
		case EPCGExPointBoundsSource::ScaledBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					if (!PointFilterCache[Index]) { return; }

					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					RegisterPointBounds(Index, new PCGExDiscardByOverlap::FPointBounds(Index, Point, Point.GetLocalBounds().TransformBy(Point.Transform)));
				}, NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					if (!PointFilterCache[Index]) { return; }

					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					RegisterPointBounds(Index, new PCGExDiscardByOverlap::FPointBounds(Index, Point, Point.GetLocalDensityBounds().TransformBy(Point.Transform)));
				}, NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);
			break;
		case EPCGExPointBoundsSource::Bounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					if (!PointFilterCache[Index]) { return; }

					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					FTransform TR = Point.Transform;
					TR.SetScale3D(FVector::OneVector); // Zero-out scale. I'm not sure this mode is of any use.
					RegisterPointBounds(Index, new PCGExDiscardByOverlap::FPointBounds(Index, Point, Point.GetLocalBounds().TransformBy(TR)));
				}, NumPoints, PrimaryFilters ? GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize() : 1024, true);
			break;
		}

		return true;
	}

	void FProcessor::ResolveOverlap(const int32 Index)
	{
		// For each managed overlap, find per-point intersections

		FOverlap* ManagedOverlap = ManagedOverlaps[Index];
		const FProcessor* OtherProcessor = static_cast<FProcessor*>(*ParentBatch->SubProcessorMap->Find(ManagedOverlap->GetOther(this)->PointDataFacade->Source));

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
						if (LocalSettings->ThresholdMeasure == EPCGExMeanMeasure::Relative)
						{
							if ((OverlapSize / ((OwnedPoint->Bounds.SphereRadius + OtherPoint->Bounds.SphereRadius) * 0.5)) < LocalSettings->MinThreshold) { return; }
						}
						else
						{
							if (OverlapSize < LocalSettings->MinThreshold) { return; }
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
		PCGEX_OUTPUT_VALUE(RelativeOverlapSubCount, Index, static_cast<double>(TOC) / LocalTypedContext->SharedOverlapSubCountMax)
		PCGEX_OUTPUT_VALUE(RelativeOverlapCount, Index, static_cast<double>(UOC) / LocalTypedContext->SharedOverlapCountMax)
	}

	void FProcessor::CompleteWork()
	{
		// 2 - Find overlaps between large bounds, we'll be searching only there.

		PCGEX_ASYNC_GROUP(AsyncManagerPtr, PreparationTask)
		PreparationTask->SetOnCompleteCallback(
			[&]()
			{
				PCGEX_ASYNC_GROUP_CHECKED(AsyncManagerPtr, SearchTask)
				SearchTask->SetOnCompleteCallback(
					[&]()
					{
						for (int i = 0; i < NumPoints; ++i)
						{
							LocalOverlapSubCountMax = FMath::Max(LocalOverlapSubCountMax, OverlapSubCount[i]);
							LocalOverlapCountMax = FMath::Max(LocalOverlapCountMax, OverlapCount[i]);
						}
					});

				SearchTask->StartRanges(
					[&](const int32 Index, const int32 Count, const int32 LoopIdx) { ResolveOverlap(Index); },
					ManagedOverlaps.Num(), 8);
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
		PCGEX_ASYNC_GROUP(AsyncManagerPtr, SearchTask)
		SearchTask->SetOnCompleteCallback(
			[&]()
			{
				PointDataFacade->Write(AsyncManagerPtr, true);

				if (LocalSettings->bTagIfHasAnyOverlap && bAnyOverlap) { PointIO->Tags->Add(LocalSettings->HasAnyOverlapTag); }
				if (LocalSettings->bTagIfHasNoOverlap && !bAnyOverlap) { PointIO->Tags->Add(LocalSettings->HasNoOverlapTag); }
			});

		SearchTask->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx) { WriteSingleData(Index); },
			NumPoints, ParentBatch->ProcessorFacades.Num());
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
