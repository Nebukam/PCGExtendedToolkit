// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBoundsToPoints.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExBoundsToPointsElement"
#define PCGEX_NAMESPACE BoundsToPoints

PCGExData::EInit UPCGExBoundsToPointsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExBoundsToPointsContext::~FPCGExBoundsToPointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(BoundsToPoints)

bool FPCGExBoundsToPointsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)

	return true;
}

bool FPCGExBoundsToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }


		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBoundsToPoints::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExBoundsToPoints::FProcessor>* NewBatch)
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

namespace PCGExBoundsToPoints
{
	FProcessor::~FProcessor()
	{
		
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BoundsToPoints)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BoundsToPoints)
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
