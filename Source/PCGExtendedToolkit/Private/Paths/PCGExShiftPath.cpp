// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExShiftPath.h"

#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExShiftPathElement"
#define PCGEX_NAMESPACE ShiftPath

UPCGExShiftPathSettings::UPCGExShiftPathSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
	bSupportCustomDirection = true;
}

PCGExData::EInit UPCGExShiftPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(ShiftPath)

bool FPCGExShiftPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ShiftPath)
	return true;
}


bool FPCGExShiftPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExShiftPathElement::Execute);

	PCGEX_CONTEXT(ShiftPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExShiftPath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExShiftPath::FProcessor>>& NewBatch)
			{
				NewBatch->bPrefetchData = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainBatch->Output();
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExShiftPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExShiftPath::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bInvertOrientation = !Context->ProcessingDirection.StartWithFirstIndex(PointDataFacade->Source);
		MaxIndex = PointDataFacade->GetNum(PCGExData::ESource::In) - 1;

		return true;
	}

	void FProcessor::CompleteWork()
	{
		// Shift!
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
