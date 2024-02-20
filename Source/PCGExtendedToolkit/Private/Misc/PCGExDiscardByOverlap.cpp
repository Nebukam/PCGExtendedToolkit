// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByOverlap.h"

#include "Data/PCGExData.h"
#include "Misc/PCGExPointsToBounds.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByOverlapElement"
#define PCGEX_NAMESPACE DiscardByOverlap

PCGExData::EInit UPCGExDiscardByOverlapSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExDiscardByOverlapContext::~FPCGExDiscardByOverlapContext()
{
	PCGEX_TERMINATE_ASYNC
	PCGEX_DELETE_TARRAY(IOBounds)
}

UPCGExDiscardByOverlapSettings::UPCGExDiscardByOverlapSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR
void UPCGExDiscardByOverlapSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
void FPCGExDiscardByOverlapContext::OutputFBounds(const PCGExPointsToBounds::FBounds* Bounds, const int32 RemoveAt)
{
	Bounds->PointIO->InitializeOutput(PCGExData::EInit::Forward);
	Bounds->PointIO->OutputTo(this);
	delete Bounds;

	if (RemoveAt != -1) { IOBounds.RemoveAt(RemoveAt); }
}

void FPCGExDiscardByOverlapContext::RemoveFBounds(const PCGExPointsToBounds::FBounds* Bounds, TArray<PCGExPointsToBounds::FBounds*>& OutAffectedBounds)
{
	for (PCGExPointsToBounds::FBounds* OtherBounds : Bounds->Overlaps)
	{
		OtherBounds->RemoveOverlap(Bounds);
		OutAffectedBounds.Add(OtherBounds);
	}

	delete Bounds;
}

PCGEX_INITIALIZE_ELEMENT(DiscardByOverlap)

bool FPCGExDiscardByOverlapElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGExPointsToBounds::ComputeBounds(Context->GetAsyncManager(), Context->MainPoints, Context->IOBounds, Settings->BoundsSource);
		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC
		Context->SetState(PCGExDiscardByOverlap::State_InitialOverlap);
	}

	if (Context->IsState(PCGExDiscardByOverlap::State_InitialOverlap))
	{
		auto CheckInitialOverlaps = [&](const int32 Index)
		{
			PCGExPointsToBounds::FBounds* Bounds = Context->IOBounds[Index];
			for (PCGExPointsToBounds::FBounds* OtherBounds : Context->IOBounds)
			{
				if (Bounds == OtherBounds) { continue; }
				if (Bounds->Bounds.Intersect(OtherBounds->Bounds))
				{
					FBox Overlap = Bounds->Bounds.Overlap(OtherBounds->Bounds);
					Bounds->FastOverlapAmount += Overlap.GetExtent().Length();
					Bounds->FastOverlaps.Add(OtherBounds, Overlap);
					Bounds->Overlaps.Add(OtherBounds);
				}
			}
		};

		if (!Context->Process(CheckInitialOverlaps, Context->IOBounds.Num())) { return false; }

		// Output sets with no overlaps
		for (int i = 0; i < Context->IOBounds.Num(); i++)
		{
			const PCGExPointsToBounds::FBounds* Bounds = Context->IOBounds[i];
			if (Bounds->FastOverlaps.IsEmpty())
			{
				Context->OutputFBounds(Bounds, i);
				i--;
			}
		}

		// No overlaps at all.
		if (Context->IOBounds.IsEmpty()) { return true; }

		if (Settings->TestMode == EPCGExOverlapTestMode::Fast)
		{
			Context->SetState(PCGExDiscardByOverlap::State_ProcessFastOverlap);
		}
		else
		{
			for (PCGExPointsToBounds::FBounds* Bounds : Context->IOBounds) { Bounds->Overlaps.Empty(); }
			
			// Compute precise overlap
			for (PCGExPointsToBounds::FBounds* Bounds : Context->IOBounds)
			{
				Context->GetAsyncManager()->Start<FPCGExComputePreciseOverlap>(Bounds->PointIO->IOIndex, Bounds->PointIO, Settings->BoundsSource, Bounds);
			}

			Context->SetAsyncState(PCGExDiscardByOverlap::State_PreciseOverlap);
		}
	}

	if (Context->IsState(PCGExDiscardByOverlap::State_PreciseOverlap))
	{
		PCGEX_WAIT_ASYNC

		// Remove non-overlapping data
		for (int i = 0; i < Context->IOBounds.Num(); i++)
		{
			const PCGExPointsToBounds::FBounds* Bounds = Context->IOBounds[i];
			if (Bounds->Overlaps.IsEmpty())
			{
				Context->OutputFBounds(Bounds, i);
				i--;
			}
		}

		if (Context->IOBounds.IsEmpty()) { return true; }

		Context->SetAsyncState(PCGExDiscardByOverlap::State_ProcessPreciseOverlap);
	}

	if (Context->IsState(PCGExDiscardByOverlap::State_ProcessFastOverlap))
	{
		auto SortBounds = [&]()
		{
			if (Settings->PruningOrder == EPCGExOverlapPruningOrder::OverlapCount) { PCGExDiscardByOverlap::SortOverlapCount(Context->IOBounds, Settings->Order); }
			else { PCGExDiscardByOverlap::SortFastAmount(Context->IOBounds, Settings->Order); }
		};

		SortBounds();

		while (!Context->IOBounds.IsEmpty())
		{
			const PCGExPointsToBounds::FBounds* CurrentBounds = Context->IOBounds.Pop();

			TArray<PCGExPointsToBounds::FBounds*> AffectedBounds;
			Context->RemoveFBounds(CurrentBounds, AffectedBounds);

			for (PCGExPointsToBounds::FBounds* AffectedBound : AffectedBounds)
			{
				if (AffectedBound->Overlaps.IsEmpty()) { Context->OutputFBounds(AffectedBound, Context->IOBounds.IndexOfByKey(AffectedBound)); }
			}

			if (Context->IOBounds.IsEmpty()) { break; }

			// Sort again
			SortBounds();
		}

		Context->Done();
	}

	if (Context->IsState(PCGExDiscardByOverlap::State_ProcessPreciseOverlap))
	{
		
		auto SortBounds = [&]()
		{
			if (Settings->PruningOrder == EPCGExOverlapPruningOrder::OverlapCount)
			{
				if (Settings->bUsePerPointsValues) { PCGExDiscardByOverlap::SortPreciseCount(Context->IOBounds, Settings->Order); }
				else { PCGExDiscardByOverlap::SortOverlapCount(Context->IOBounds, Settings->Order); }
			}
			else { PCGExDiscardByOverlap::SortPreciseAmount(Context->IOBounds, Settings->Order); }
		};

		SortBounds();

		while (!Context->IOBounds.IsEmpty())
		{
			const PCGExPointsToBounds::FBounds* CurrentBounds = Context->IOBounds.Pop();

			TArray<PCGExPointsToBounds::FBounds*> AffectedBounds;
			Context->RemoveFBounds(CurrentBounds, AffectedBounds);

			for (PCGExPointsToBounds::FBounds* AffectedBound : AffectedBounds)
			{
				if (AffectedBound->Overlaps.IsEmpty()) { Context->OutputFBounds(AffectedBound, Context->IOBounds.IndexOfByKey(AffectedBound)); }
			}

			if (Context->IOBounds.IsEmpty()) { break; }

			// Sort again
			SortBounds();
		}

		Context->Done();
	}

	return Context->IsDone();
}

bool FPCGExComputePreciseOverlap::ExecuteTask()
{
	FPCGExDiscardByOverlapContext* Context = static_cast<FPCGExDiscardByOverlapContext*>(Manager->Context);
	PCGEX_SETTINGS(DiscardByOverlap)

	const TArray<FPCGPoint>& InPoints = Bounds->PointIO->GetIn()->GetPoints();
	TArray<PCGExPointsToBounds::FBounds*> NonOverlappingBounds;

	const double StaticExpansion = Settings->ExpansionMode == EPCGExExpandPointsBoundsMode::Static ? Settings->ExpansionValue : 0;
	PCGEx::FLocalSingleFieldGetter* LocalBoundsExpansion = new PCGEx::FLocalSingleFieldGetter();
	LocalBoundsExpansion->Capture(Settings->ExpansionLocalValue);
	LocalBoundsExpansion->SoftGrab(*Bounds->PointIO);

	for (const TPair<PCGExPointsToBounds::FBounds*, FBox> Overlap : Bounds->FastOverlaps)
	{
		if (Bounds->PreciseOverlapAmount.Find(Overlap.Key)) { continue; } // Already processed

		const TArray<FPCGPoint>& OtherPoints = Overlap.Key->PointIO->GetIn()->GetPoints();
		const UPCGPointData::PointOctree& Octree = Bounds->PointIO->GetIn()->GetOctree();

		TArray<int32> OtherIndices;

		auto ProcessOtherPoint = [&](const FPCGPointRef& InPointRef)
		{
			const ptrdiff_t PointIndex = InPointRef.Point - OtherPoints.GetData();
			if (PointIndex < 0 && PointIndex >= OtherPoints.Num()) { return; }
			OtherIndices.Add(PointIndex);
		};

		const UPCGPointData::PointOctree& OtherOctree = Overlap.Key->PointIO->GetIn()->GetOctree();
		OtherOctree.FindElementsWithBoundsTest(Overlap.Value, ProcessOtherPoint);

		if (OtherIndices.IsEmpty())
		{
			// No overlap with this key
			NonOverlappingBounds.Add(Overlap.Key);
			continue;
		}

		int32 CurrentOverlapCount = 0;
		double CurrentOverlapAmount = 0;

		auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
		{
			const ptrdiff_t PointIndex = InPointRef.Point - OtherPoints.GetData();
			if (PointIndex < 0 && PointIndex >= OtherPoints.Num()) { return; }

			const FBox A = PCGExPointsToBounds::GetBounds(InPoints[PointIndex], BoundsSource).ExpandBy(LocalBoundsExpansion->SoftGet(*InPointRef.Point, StaticExpansion));;

			for (const int32 OtherIndex : OtherIndices)
			{
				const FBox B = PCGExPointsToBounds::GetBounds(OtherPoints[OtherIndex], BoundsSource);

				if (!A.Intersect(B)) { continue; }

				CurrentOverlapCount++;
				CurrentOverlapAmount += A.Overlap(B).GetExtent().Length();
			}
		};

		Octree.FindElementsWithBoundsTest(Overlap.Value, ProcessPoint);

		if (CurrentOverlapCount == 0)
		{
			// No overlap with this key
			NonOverlappingBounds.Add(Overlap.Key);
			continue;
		}

		Bounds->Overlaps.Add(Overlap.Key);

		Bounds->PreciseOverlapCount.Add(Overlap.Key, CurrentOverlapCount);
		Bounds->PreciseOverlapAmount.Add(Overlap.Key, CurrentOverlapAmount);

		Bounds->TotalPreciseOverlapCount += CurrentOverlapCount;
		Bounds->TotalPreciseOverlapAmount += CurrentOverlapAmount;
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
