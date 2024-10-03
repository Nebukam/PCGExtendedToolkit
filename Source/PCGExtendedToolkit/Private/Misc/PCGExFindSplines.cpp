// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFindSplines.h"


#define LOCTEXT_NAMESPACE "PCGExFindSplinesElement"
#define PCGEX_NAMESPACE FindSplines

PCGExData::EInit UPCGExFindSplinesSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(FindSplines)

bool FPCGExFindSplinesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindSplines)

	PCGEX_VALIDATE_NAME(Settings->ActorReferenceAttributeName)

	return true;
}

bool FPCGExFindSplinesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindSplinesElement::Execute);

	PCGEX_CONTEXT(FindSplines)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFindSplines::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFindSplines::FProcessor>>& NewBatch)
			{
			}))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch(PCGExMT::State_Done)) { return false; }

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFindSplines
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindSplines::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
	}

	void FProcessor::CompleteWork()
	{
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
