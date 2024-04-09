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
		ComputeBounds(Context->GetAsyncManager(), Context->MainPoints, Context->IOBounds, Settings->BoundsSource);
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
					const double L = Overlap.GetExtent().Length();
					Bounds->FastOverlapAmount += L - FMath::Fmod(L, Settings->AmountFMod);
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
			if (Bounds->Overlaps.IsEmpty())
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

		Context->SetState(PCGExDiscardByOverlap::State_ProcessPreciseOverlap);
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
				if (AffectedBound->Overlaps.IsEmpty())
				{
					Context->OutputFBounds(AffectedBound, Context->IOBounds.IndexOfByKey(AffectedBound));
				}
			}

			AffectedBounds.Empty();

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
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExComputePreciseOverlap::ExecuteTask);

	const FPCGExDiscardByOverlapContext* Context = static_cast<FPCGExDiscardByOverlapContext*>(Manager->Context);
	PCGEX_SETTINGS(DiscardByOverlap)

	const TArray<FPCGPoint>& LocalPoints = Bounds->PointIO->GetIn()->GetPoints();

	const double StaticExpansion = Settings->ExpansionMode == EPCGExExpandPointsBoundsMode::Static ? Settings->ExpansionValue : 0;

	PCGEx::FLocalSingleFieldGetter* BoundsExpansion = new PCGEx::FLocalSingleFieldGetter();
	BoundsExpansion->Capture(Settings->ExpansionLocalValue);
	BoundsExpansion->SoftGrab(*Bounds->PointIO);

	for (const TPair<PCGExPointsToBounds::FBounds*, FBox> Overlap : Bounds->FastOverlaps)
	{
		PCGExPointsToBounds::FBounds* OtherBounds = Overlap.Key;
		if (Bounds->OverlapsWith(OtherBounds)) { continue; } // Already processed

		TArray<FBox> LocalBounds;
		auto ProcessLocalPoints = [&](const FPCGPointRef& LocalPointRef)
		{
			const ptrdiff_t LocalIndex = LocalPointRef.Point - LocalPoints.GetData();
			if (!LocalPoints.IsValidIndex(LocalIndex)) { return; }

			const FPCGPoint& Pt = LocalPoints[LocalIndex];
			//LocalBounds.Add(PCGExPointsToBounds::GetBounds(Pt, BoundsSource).ExpandBy(BoundsExpansion->SoftGet(Pt, StaticExpansion)));
			LocalBounds.Add(PCGExPointsToBounds::GetBounds(Pt, BoundsSource));
		};

		Bounds->PointIO->GetIn()->GetOctree().FindElementsWithBoundsTest(Overlap.Value, ProcessLocalPoints);

		if (LocalBounds.IsEmpty()) { continue; } // No overlap with this key

		int32 OverlapCount = 0;
		double OverlapAmount = 0;

		const TArray<FPCGPoint>& OtherPoints = OtherBounds->PointIO->GetIn()->GetPoints();

		auto ProcessOtherPoint = [&](const FPCGPointRef& OtherPointRef)
		{
			const ptrdiff_t OtherPointIndex = OtherPointRef.Point - OtherPoints.GetData();
			if (!OtherPoints.IsValidIndex(OtherPointIndex)) { return; }

			const FBox OtherBox = PCGExPointsToBounds::GetBounds(OtherPoints[OtherPointIndex], BoundsSource);

			for (const FBox LocalBox : LocalBounds)
			{
				if (!LocalBox.Intersect(OtherBox)) { continue; }

				OverlapCount++;
				const double L = LocalBox.Overlap(OtherBox).GetExtent().Length();
				OverlapAmount += L - FMath::Fmod(L, Settings->AmountFMod);
			}
		};

		OtherBounds->PointIO->GetIn()->GetOctree().FindElementsWithBoundsTest(Overlap.Value, ProcessOtherPoint);

		if (OverlapCount == 0) { continue; } // No overlap with this key

		Bounds->AddPreciseOverlap(OtherBounds, OverlapCount, OverlapAmount);
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
