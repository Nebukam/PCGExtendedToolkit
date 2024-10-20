// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExFindPointOnBounds.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExFindPointOnBoundsElement"
#define PCGEX_NAMESPACE FindPointOnBounds

PCGExData::EInit UPCGExFindPointOnBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(FindPointOnBounds)

bool FPCGExFindPointOnBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBounds)

	return true;
}

bool FPCGExFindPointOnBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindPointOnBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FindPointOnBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExFindPointOnBounds::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExFindPointOnBounds::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExFindPointOnBounds
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFindPointOnBounds::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		const FBox Bounds = PointDataFacade->Source->GetIn()->GetBounds();
		const FVector E = Bounds.GetExtent();
		SearchPosition = Bounds.GetCenter() + FVector(Settings->UConstant * E.X, Settings->VConstant * E.Y, Settings->WConstant * E.Z);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (const double Dist = FVector::Dist(Point.Transform.GetLocation(), SearchPosition); Dist < BestDistance)
		{
			FWriteScopeLock WriteLock(BestIndexLock);
			BestIndex = Index;
			BestDistance = Dist;
		}
	}

	void FProcessor::CompleteWork()
	{
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
