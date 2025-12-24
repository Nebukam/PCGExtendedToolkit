// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExRefreshSeed.h"


#include "Helpers/PCGExRandomHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExRefreshSeedElement"
#define PCGEX_NAMESPACE RefreshSeed

PCGEX_INITIALIZE_ELEMENT(RefreshSeed)

bool FPCGExRefreshSeedElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RefreshSeed)

	return true;
}

class FPCGExRefreshSeedTask final : public PCGExMT::FPCGExIndexedTask
{
public:
	explicit FPCGExRefreshSeedTask(const int32 InPointIndex, const TSharedPtr<PCGExData::FPointIO>& InPointIO)
		: FPCGExIndexedTask(InPointIndex), PointIO(InPointIO)
	{
	}

	const TSharedPtr<PCGExData::FPointIO> PointIO;

	virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
	{
		PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::Duplicate)

		TPCGValueRange<int32> Seeds = PointIO->GetOut()->GetSeedValueRange();
		TConstPCGValueRange<FTransform> Transforms = PointIO->GetOut()->GetConstTransformValueRange();

		const FVector BaseOffset = FVector(TaskIndex) * 0.001;
		for (int i = 0; i < PointIO->GetNum(); i++) { Seeds[i] = PCGExRandomHelpers::ComputeSpatialSeed(Transforms[i].GetLocation(), BaseOffset); }
	}
};

bool FPCGExRefreshSeedElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRefreshSeedElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RefreshSeed)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		const TSharedPtr<PCGExMT::FTaskManager> TaskManager = Context->GetTaskManager();
		while (Context->AdvancePointsIO(false))
		{
			PCGEX_LAUNCH(FPCGExRefreshSeedTask, Settings->Base + Context->CurrentIO->IOIndex, Context->CurrentIO)
		}

		Context->SetState(PCGExCommon::States::State_WaitingOnAsyncWork);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_WaitingOnAsyncWork)
	{
		Context->Done();
		Context->MainPoints->StageOutputs();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
