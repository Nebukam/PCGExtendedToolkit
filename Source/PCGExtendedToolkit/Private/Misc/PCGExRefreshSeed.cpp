// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExRefreshSeed.h"

#include "PCGExRandom.h"


#define LOCTEXT_NAMESPACE "PCGExRefreshSeedElement"
#define PCGEX_NAMESPACE RefreshSeed

PCGExData::EIOInit UPCGExRefreshSeedSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(RefreshSeed)

bool FPCGExRefreshSeedElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefreshSeed)

	return true;
}

bool FPCGExRefreshSeedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefreshSeedElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefreshSeed)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		while (Context->AdvancePointsIO(false))
		{
			Context->GetAsyncManager()->Start<FPCGExRefreshSeedTask>(Settings->Base + Context->CurrentIO->IOIndex, Context->CurrentIO);
		}

		Context->SetAsyncState(PCGEx::State_WaitingOnAsyncWork);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_WaitingOnAsyncWork)
	{
		Context->Done();
		Context->MainPoints->StageOutputs();
	}

	return Context->TryComplete();
}

bool FPCGExRefreshSeedTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

	const FVector BaseOffset = FVector(TaskIndex) * 0.001;
	for (int i = 0; i < PointIO->GetNum(); i++) { MutablePoints[i].Seed = PCGExRandom::ComputeSeed(MutablePoints[i], BaseOffset); }

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
