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

	OverlapMap.Empty();
	PCGEX_DELETE_TARRAY(Overlaps)
}

PCGExDiscardByOverlap::FOverlap* FPCGExDiscardByOverlapContext::RegisterOverlap(
	const PCGExData::FFacade* InManager,
	const PCGExData::FFacade* InManaged,
	const FBox& Intersection)
{
	const uint64 OID = PCGEx::H64U(InManager->Source->IOIndex, InManaged->Source->IOIndex);

	{
		FReadScopeLock ReadScopeLock(OverlapLock);
		if (PCGExDiscardByOverlap::FOverlap** OverlapPtr = OverlapMap.Find(OID)) { return *OverlapPtr; }
	}

	{
		FReadScopeLock WriteScopeLock(OverlapLock);
		if (PCGExDiscardByOverlap::FOverlap** OverlapPtr = OverlapMap.Find(OID)) { return *OverlapPtr; }

		PCGExDiscardByOverlap::FOverlap* Overlap = new PCGExDiscardByOverlap::FOverlap(InManager, InManaged, Intersection);
		Overlaps.Add(Overlap);
		OverlapMap.Add(OID, Overlap);
		return Overlap;
	}
}

PCGExDiscardByOverlap::FOverlap* FPCGExDiscardByOverlapContext::GetOverlap(
	const PCGExData::FFacade* InManager,
	const PCGExData::FFacade* InManaged)
{
	const uint64 OID = PCGEx::H64U(InManager->Source->IOIndex, InManaged->Source->IOIndex);
	return *OverlapMap.Find(OID);
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

	while (!Context->Overlaps.IsEmpty())
	{
		// Sort remaining overlaps...
	}

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExDiscardByOverlap
{
	void FProcessor::RegisterOverlap(FOverlap* InOverlap)
	{
		FWriteScopeLock WriteLock(OverlapLock);
		Overlaps.Add(InOverlap);
		OverlapsMap.Add(InOverlap->GetOtherFacade(PointDataFacade)->Source->IOIndex, InOverlap);
		if (InOverlap->Manager == PointDataFacade) { ManagedOverlaps.Add(InOverlap); }
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Octree)
		PCGEX_DELETE_TARRAY(LocalPointBounds)

		Overlaps.Empty();
		ManagedOverlaps.Empty();
		OverlapsMap.Empty();
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
					const FVector S = Point.Transform.GetScale3D();
					const FBox PB = Point.GetLocalBounds();
					const FBox Box = FBox(PB.Min * S, PB.Max * S);

					FPointBounds* NewPointBounds = new FPointBounds(Point, Box);
					LocalPointBounds[Index] = NewPointBounds;

					Bounds += NewPointBounds->WorldBounds.GetBox();
				}, ParentBatch->ProcessorFacades.Num(), 64, true);
			break;
		case EPCGExPointBoundsSource::DensityBounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					const FVector S = Point.Transform.GetScale3D();
					const FBox PB = Point.GetLocalDensityBounds();
					const FBox Box = FBox(PB.Min * S, PB.Max * S);

					FPointBounds* NewPointBounds = new FPointBounds(Point, Box);
					LocalPointBounds[Index] = NewPointBounds;

					Bounds += NewPointBounds->WorldBounds.GetBox();
				}, ParentBatch->ProcessorFacades.Num(), 64, true);
			break;
		case EPCGExPointBoundsSource::Bounds:
			BoundsPreparationTask->StartRanges(
				[&](const int32 Index, const int32 Count, const int32 LoopIdx)
				{
					const FPCGPoint& Point = *(InPoints->GetData() + Index);
					const FBox Box = Point.GetLocalBounds();

					FPointBounds* NewPointBounds = new FPointBounds(Point, Box);
					LocalPointBounds[Index] = NewPointBounds;

					Bounds += NewPointBounds->WorldBounds.GetBox();
				}, ParentBatch->ProcessorFacades.Num(), 64, true);
			break;
		}

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		// For each managed overlap, find per-point intersections

		FOverlap* ManagedOverlap = ManagedOverlaps[Iteration];
		const PCGExData::FFacade* OtherFacade = ManagedOverlap->GetOtherFacade(PointDataFacade);
		const FProcessor* OtherProcessor = static_cast<FProcessor*>(*ParentBatch->SubProcessorMap->Find(ManagedOverlap->GetOtherFacade(PointDataFacade)->Source));

		Octree->FindElementsWithBoundsTest(
			FBoxCenterAndExtent(ManagedOverlap->Intersection.GetCenter(), ManagedOverlap->Intersection.GetExtent()),
			[&](const FPointBounds* OwnedPoint)
			{
				const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(OwnedPoint->WorldBounds.GetBox());
				OtherProcessor->GetOctree()->FindElementsWithBoundsTest(
					BCAE,
					[&](const FPointBounds* OtherPoint)
					{
						// Check if there is any overlap between OwnedPoint and OtherPoint
						// If so, register it
						// This is safe since we manage the overlap here /o/
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
				if (LocalSettings->TestMode == EPCGExOverlapTestMode::Precise) { StartParallelLoopForRange(ManagedOverlaps.Num(), 8); }
			});
		PreparationTask->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				const PCGExData::FFacade* OtherFacade = ParentBatch->ProcessorFacades[Index];
				if (PointDataFacade == OtherFacade) { return; } // Skip self

				const FProcessor* OtherProcessor = static_cast<FProcessor*>(*ParentBatch->SubProcessorMap->Find(OtherFacade->Source));

				const FBox IntersectionBox = Bounds.Overlap(OtherProcessor->GetBounds());
				if (FMath::IsNearlyZero(IntersectionBox.GetExtent().Length())) { return; } // No overlap

				FOverlap* Overlap = LocalTypedContext->RegisterOverlap(
					PointDataFacade,
					OtherProcessor->PointDataFacade,
					IntersectionBox);

				RegisterOverlap(Overlap);
			}, ParentBatch->ProcessorFacades.Num(), 64);
	}

	void FProcessor::Write()
	{
		// Sort overlaps so we can process them
		ManagedOverlaps.Empty();

		for (const FOverlap* Overlap : Overlaps) { Stats.Add(Overlap->Stats); } // Sum up stats

		/*
		switch (LocalSettings->PruningOrder)
		{
		case EPCGExOverlapPruningOrder::OverlapCount:
			switch (LocalSettings->Order)
			{
			case EPCGExSortDirection::Ascending:
				Overlaps.Sort([](const FOverlap* A, const FOverlap* B) { return A->Stats.OverlapCount > B->Stats.OverlapCount; });
				break;
			case EPCGExSortDirection::Descending:
				Overlaps.Sort([](const FOverlap* A, const FOverlap* B) { return A->Stats.OverlapCount < B->Stats.OverlapCount; });
				break;
			default: ;
			}
			break;
		case EPCGExOverlapPruningOrder::OverlapAmount:
			switch (LocalSettings->Order)
			{
			case EPCGExSortDirection::Ascending:
				Overlaps.Sort([](const FOverlap* A, const FOverlap* B) { return A->Stats.OverlapAmount > B->Stats.OverlapAmount; });
				break;
			case EPCGExSortDirection::Descending:
				Overlaps.Sort([](const FOverlap* A, const FOverlap* B) { return A->Stats.OverlapAmount < B->Stats.OverlapAmount; });
				break;
			default: ;
			}
			break;
		default: ;
		}
		*/
	}

	int32 FProcessor::RemoveOverlap(const PCGExData::FFacade* InFacade)
	{
		FOverlap** OverlapPtr = OverlapsMap.Find(InFacade->Source->IOIndex);
		if (!OverlapPtr) { return Overlaps.Num(); }

		OverlapsMap.Remove(InFacade->Source->IOIndex);
		FOverlap* Overlap = *OverlapPtr;
		Overlaps.Remove(Overlap);
		Stats.Remove(Overlap->Stats);

		return Overlaps.Num();
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
