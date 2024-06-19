// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExRefreshSeed.h"

#define LOCTEXT_NAMESPACE "PCGExRefreshSeedElement"
#define PCGEX_NAMESPACE RefreshSeed

PCGExData::EInit UPCGExRefreshSeedSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(RefreshSeed)

bool FPCGExRefreshSeedElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefreshSeed)

	return true;
}

bool FPCGExRefreshSeedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefreshSeedElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefreshSeed)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		while (Context->AdvancePointsIO(false))
		{
			Context->GetAsyncManager()->Start<FPCGExRefreshSeedTask>(Settings->Base + Context->CurrentIO->IOIndex, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		Context->Done();
		Context->OutputMainPoints();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}

bool FPCGExRefreshSeedTask::ExecuteTask()
{
	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

	const FVector BaseOffset = FVector(TaskIndex) * 0.001;
	for (int i = 0; i < PointIO->GetNum(); i++) { PCGExMath::RandomizeSeed(MutablePoints[i], BaseOffset); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
