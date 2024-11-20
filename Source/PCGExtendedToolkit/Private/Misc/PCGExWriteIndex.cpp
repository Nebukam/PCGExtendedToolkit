// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"


#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"
#define PCGEX_NAMESPACE WriteIndex

PCGExData::EIOInit UPCGExWriteIndexSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(WriteIndex)

bool FPCGExWriteIndexElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)

	PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)

	if (Settings->bOutputCollectionIndex)
	{
		PCGEX_VALIDATE_NAME(Settings->CollectionIndexAttributeName)
	}

	return true;
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	PCGEX_CONTEXT(WriteIndex)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExWriteIndex
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		NumPoints = PointDataFacade->GetNum();

		if (Settings->bOutputNormalizedIndex)
		{
			DoubleWriter = PointDataFacade->GetWritable<double>(Settings->OutputAttributeName, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::Inherit);
		}
		else
		{
			IntWriter = PointDataFacade->GetWritable<int32>(Settings->OutputAttributeName, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::Inherit);
		}

		if (Settings->bOutputCollectionIndex)
		{
			WriteMark(PointDataFacade->Source, Settings->CollectionIndexAttributeName, BatchIndex);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (DoubleWriter) { DoubleWriter->GetMutable(Index) = static_cast<double>(Index) / NumPoints; }
		else { IntWriter->GetMutable(Index) = Index; }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
