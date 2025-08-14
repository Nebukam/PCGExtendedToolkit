// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExRefreshSeed.h"

#include "PCGExRandom.h"
#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExRefreshSeedElement"
#define PCGEX_NAMESPACE RefreshSeed

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
		const TSharedPtr<PCGExMT::FTaskManager> AsyncManager = Context->GetAsyncManager();
		while (Context->AdvancePointsIO(false))
		{
			PCGEX_LAUNCH(FPCGExRefreshSeedTask, Settings->Base + Context->CurrentIO->IOIndex, Context->CurrentIO)
		}

		Context->SetAsyncState(PCGExCommon::State_WaitingOnAsyncWork);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_WaitingOnAsyncWork)
	{
		Context->Done();
		Context->MainPoints->StageOutputs();
	}

	return Context->TryComplete();
}

void FPCGExRefreshSeedTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::Duplicate)

	TPCGValueRange<int32> Seeds = PointIO->GetOut()->GetSeedValueRange();
	TConstPCGValueRange<FTransform> Transforms = PointIO->GetOut()->GetConstTransformValueRange();

	const FVector BaseOffset = FVector(TaskIndex) * 0.001;
	for (int i = 0; i < PointIO->GetNum(); i++) { Seeds[i] = PCGExRandom::ComputeSpatialSeed(Transforms[i].GetLocation(), BaseOffset); }
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
