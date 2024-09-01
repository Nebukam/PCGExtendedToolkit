// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByOverlap.h"

#include "Misc/PCGExPointsToBounds.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByOverlapElement"
#define PCGEX_NAMESPACE DiscardByOverlap

PCGExData::EInit UPCGExDiscardByOverlapSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

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

PCGEX_INITIALIZE_ELEMENT(DiscardByOverlap)

bool FPCGExDiscardByOverlapElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DiscardByOverlap)

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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExDiscardByOverlap::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExDiscardByOverlap::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true; // Not really but we need the step
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any input to check for overlaps."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	TArray<PCGExDiscardByOverlap::FProcessor*> ProcessorStack;
	ProcessorStack.Reserve(Context->MainBatch->GetNumProcessors());

	for (const TPair<PCGExData::FPointIO*, PCGExPointsMT::FPointsProcessor*> Pair : *(Context->MainBatch->SubProcessorMap))
	{
		if (!Pair.Value->bIsProcessorValid) { continue; }
		ProcessorStack.Add(static_cast<PCGExDiscardByOverlap::FProcessor*>(Pair.Value));
	}

	while (!ProcessorStack.IsEmpty())
	{
		// Sort remaining overlaps...

#define PCGEX_COMPARE_AB(_FUNC) \
		switch (Settings->Logic){ default: case EPCGExOverlapPruningLogic::LowFirst: \
			ProcessorStack.Sort([](const PCGExDiscardByOverlap::FProcessor& A, const PCGExDiscardByOverlap::FProcessor& B) { return A._FUNC(B); }); \
			break; case EPCGExOverlapPruningLogic::HighFirst: \
			ProcessorStack.Sort([](const PCGExDiscardByOverlap::FProcessor& A, const PCGExDiscardByOverlap::FProcessor& B) { return !A._FUNC(B); }); \
			break; }

		switch (Settings->PruningReference)
		{
		default:
		case EPCGExOverlapCompare::Count:
			PCGEX_COMPARE_AB(CompareOverlapCount)
			break;
		case EPCGExOverlapCompare::SubCount:
			PCGEX_COMPARE_AB(CompareOverlapSubCount)
			break;
		case EPCGExOverlapCompare::Volume:
			PCGEX_COMPARE_AB(CompareOverlapVolume)
			break;
		}

#undef PCGEX_COMPARE_AB

		// 1 - Remove top/bottom of the list

		PCGExDiscardByOverlap::FProcessor* Candidate = ProcessorStack.Pop();

		if (Candidate->HasOverlaps()) { Candidate->Prune(); }
		else { Candidate->PointDataFacade->Source->InitializeOutput(PCGExData::EInit::Forward); }
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

	void FProcessor::RemoveOverlap(FOverlap* InOverlap)
	{
		Overlaps.Remove(InOverlap);
		Stats.Remove(InOverlap->Stats, NumPoints, RoughVolume);
	}

	void FProcessor::Prune()
	{
		for (FOverlap* Overlap : Overlaps)
		{
			Overlap->GetOther(this)->RemoveOverlap(Overlap);
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
				for (FPointBounds* PtBounds : LocalPointBounds) { Octree->AddElement(PtBounds); }
			});

		switch (Settings->BoundsSource)
		{
		default:
		case EPCGExPointBoundsSource::ScaledBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					RegisterPointBounds(Index, new FPointBounds(Point, Point.GetLocalBounds().TransformBy(Point.Transform)));
				}, NumPoints, 1024, true);
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					RegisterPointBounds(Index, new FPointBounds(Point, Point.GetLocalDensityBounds().TransformBy(Point.Transform)));
				}, NumPoints, 1024, true);
			break;
		case EPCGExPointBoundsSource::Bounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					FTransform TR = Point.Transform;
					TR.SetScale3D(FVector::OneVector); // Zero-out scale. I'm not sure this mode is of any use.
					RegisterPointBounds(Index, new FPointBounds(Point, Point.GetLocalBounds().TransformBy(TR)));
				}, NumPoints, 1024, true);
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
		// Sort overlaps so we can process them
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

		Stats.UpdateRelative(NumPoints, RoughVolume);
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
