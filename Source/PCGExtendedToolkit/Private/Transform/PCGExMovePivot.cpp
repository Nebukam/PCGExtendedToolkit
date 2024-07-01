// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform//PCGExMovePivot.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExMovePivotElement"
#define PCGEX_NAMESPACE MovePivot

PCGExData::EInit UPCGExMovePivotSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExMovePivotContext::~FPCGExMovePivotContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(MovePivot)

bool FPCGExMovePivotElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(MovePivot)

	return true;
}

bool FPCGExMovePivotElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMovePivotElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(MovePivot)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }


		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExMovePivot::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExMovePivot::FProcessor>* NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to subdivide."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExMovePivot
{
	FProcessor::~FProcessor()
	{
		
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MovePivot)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(MovePivot)
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
