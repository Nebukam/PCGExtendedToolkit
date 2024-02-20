// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardByOverlap.h"

#include "Data/PCGExData.h"
#include "Misc/PCGExPointsToBounds.h"

#define LOCTEXT_NAMESPACE "PCGExDiscardByOverlapElement"
#define PCGEX_NAMESPACE DiscardByOverlap

PCGExData::EInit UPCGExDiscardByOverlapSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

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
		PCGExPointsToBounds::ComputeBounds(
			Context->GetAsyncManager(), Context->MainPoints,
			Context->IOBounds, Settings->BoundsSource, &Settings->LocalBoundsSource);
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
					Bounds->Overlaps.Add(OtherBounds, Overlap);
				}
			}
		};

		if (!Context->Process(CheckInitialOverlaps, Context->IOBounds.Num())) { return false; }

		double MaxOverlapAmount = TNumericLimits<double>::Min();
		for (const PCGExPointsToBounds::FBounds* Bounds : Context->IOBounds) { MaxOverlapAmount = FMath::Max(Bounds->LooseOverlapAmount, MaxOverlapAmount); }
		for (PCGExPointsToBounds::FBounds* Bounds : Context->IOBounds) { Bounds->LooseOverlapAmount /= MaxOverlapAmount; }

		if (MaxOverlapAmount <= 0)
		{
			// No overlap
			DisabledPassThroughData(Context);
			return true;
		}

		Context->SetState(PCGExMT::State_ProcessingPoints);
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
