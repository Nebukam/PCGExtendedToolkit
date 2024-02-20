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
					Bounds->LooseOverlapAmount += Overlap.GetExtent().Length();
					Bounds->FastOverlaps.Add(OtherBounds, Overlap);
				}
			}
		};

		if (!Context->Process(CheckInitialOverlaps, Context->IOBounds.Num())) { return false; }

		// Remove non-overlapping data
		for (int i = 0; i < Context->IOBounds.Num(); i++)
		{
			const PCGExPointsToBounds::FBounds* Bounds = Context->IOBounds[i];
			if (Bounds->FastOverlaps.IsEmpty())
			{
				Bounds->PointIO->InitializeOutput(PCGExData::EInit::Forward);
				Bounds->PointIO->OutputTo(Context);

				Context->IOBounds.RemoveAt(i);
				delete Bounds;
				i--;
			}
		}

		if (Context->IOBounds.IsEmpty()) { return true; }

		double MaxOverlapAmount = TNumericLimits<double>::Min();
		for (const PCGExPointsToBounds::FBounds* Bounds : Context->IOBounds) { MaxOverlapAmount = FMath::Max(Bounds->LooseOverlapAmount, MaxOverlapAmount); }
		for (PCGExPointsToBounds::FBounds* Bounds : Context->IOBounds) { Bounds->LooseOverlapAmount /= MaxOverlapAmount; }

		if (Settings->TestMode == EPCGExOverlapTestMode::Fast)
		{
			if (Settings->PruningOrder == EPCGExOverlapPruningOrder::OverlapCount)
			{
				Context->IOBounds.Sort(
					[&](const PCGExPointsToBounds::FBounds& A, const PCGExPointsToBounds::FBounds& B)
					{
						return Settings->Order == EPCGExSortDirection::Ascending ? A.FastOverlaps.Num() < B.FastOverlaps.Num() :
							       A.FastOverlaps.Num() < B.FastOverlaps.Num();
					});
			}
			else
			{
				Context->IOBounds.Sort(
					[&](const PCGExPointsToBounds::FBounds& A, const PCGExPointsToBounds::FBounds& B)
					{
						return Settings->Order == EPCGExSortDirection::Ascending ? A.LooseOverlapAmount < B.LooseOverlapAmount :
							       A.LooseOverlapAmount < B.LooseOverlapAmount;
					});
			}

			Context->SetState(PCGExDiscardByOverlap::State_ProcessFastOverlap);
		}
		else
		{
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
			if (Bounds->PreciseOverlapCount.IsEmpty())
			{
				Bounds->PointIO->InitializeOutput(PCGExData::EInit::Forward);
				Bounds->PointIO->OutputTo(Context);

				Context->IOBounds.RemoveAt(i);
				delete Bounds;
				i--;
			}
		}

		if (Context->IOBounds.IsEmpty()) { return true; }

		Context->SetAsyncState(PCGExDiscardByOverlap::State_PreciseOverlap);
	}

	if (Context->IsState(PCGExDiscardByOverlap::State_ProcessFastOverlap))
	{
		//TODO: Process fast overlap results
		
		Context->Done();
	}

	if (Context->IsState(PCGExDiscardByOverlap::State_ProcessPreciseOverlap))
	{
		//TODO: Process precise overlap results
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
	int32 PreciseOverlapCount = 0;
	double PreciseOverlapAmount = 0;

	for (const TPair<PCGExPointsToBounds::FBounds*, FBox> Overlap : Bounds->FastOverlaps)
	{
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

		if (BoundsSource == EPCGExPointBoundsSource::DensityBounds)
		{
			auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t PointIndex = InPointRef.Point - OtherPoints.GetData();
				if (PointIndex < 0 && PointIndex >= OtherPoints.Num()) { return; }

				const FBox A = InPoints[PointIndex].GetDensityBounds().GetBox();

				for (const int32 OtherIndex : OtherIndices)
				{
					const FBox B = OtherPoints[OtherIndex].GetDensityBounds().GetBox();
					if (!A.Intersect(B)) { return; }

					CurrentOverlapCount++;
					CurrentOverlapAmount += A.Overlap(B).GetExtent().Length();
				}
			};

			Octree.FindElementsWithBoundsTest(Overlap.Value, ProcessPoint);
		}
		else if (BoundsSource == EPCGExPointBoundsSource::ScaledExtents)
		{
			auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t PointIndex = InPointRef.Point - OtherPoints.GetData();
				if (PointIndex < 0 && PointIndex >= OtherPoints.Num()) { return; }

				const FPCGPoint& Pt = InPoints[PointIndex];
				const FBox A = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetScaledExtents()).GetBox();

				for (const int32 OtherIndex : OtherIndices)
				{
					const FPCGPoint& PtB = OtherPoints[OtherIndex];
					const FBox B = FBoxCenterAndExtent(PtB.Transform.GetLocation(), PtB.GetScaledExtents()).GetBox();

					if (!A.Intersect(B)) { return; }

					CurrentOverlapCount++;
					CurrentOverlapAmount += A.Overlap(B).GetExtent().Length();
				}
			};

			Octree.FindElementsWithBoundsTest(Overlap.Value, ProcessPoint);
		}
		else if (BoundsSource == EPCGExPointBoundsSource::Extents)
		{
			auto ProcessPoint = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t PointIndex = InPointRef.Point - OtherPoints.GetData();
				if (PointIndex < 0 && PointIndex >= OtherPoints.Num()) { return; }

				const FPCGPoint& Pt = InPoints[PointIndex];
				const FBox A = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetExtents()).GetBox();

				for (const int32 OtherIndex : OtherIndices)
				{
					const FPCGPoint& PtB = OtherPoints[OtherIndex];
					const FBox B = FBoxCenterAndExtent(PtB.Transform.GetLocation(), PtB.GetExtents()).GetBox();

					if (!A.Intersect(B)) { return; }

					CurrentOverlapCount++;
					CurrentOverlapAmount += A.Overlap(B).GetExtent().Length();
				}
			};

			Octree.FindElementsWithBoundsTest(Overlap.Value, ProcessPoint);
		}

		if (CurrentOverlapCount == 0)
		{
			// No overlap with this key
			NonOverlappingBounds.Add(Overlap.Key);
			continue;
		}

		Bounds->PreciseOverlapCount.Add(Overlap.Key, CurrentOverlapCount);
		Bounds->PreciseOverlapAmount.Add(Overlap.Key, CurrentOverlapAmount);

		PreciseOverlapCount += CurrentOverlapCount;
		PreciseOverlapAmount += CurrentOverlapAmount;
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
