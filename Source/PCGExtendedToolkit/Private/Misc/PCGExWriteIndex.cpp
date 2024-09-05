// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"
#define PCGEX_NAMESPACE WriteIndex

PCGExData::EInit UPCGExWriteIndexSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(WriteIndex)

FPCGExWriteIndexContext::~FPCGExWriteIndexContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWriteIndexElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)

	PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)

	if(Settings->bOutputCollectionIndex)
	{
		PCGEX_VALIDATE_NAME(Settings->CollectionIndexAttributeName)
	}
	
	return true;
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	PCGEX_CONTEXT(WriteIndex)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();
	
	return Context->TryComplete();
}

namespace PCGExWriteIndex
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteIndex)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		NumPoints = PointIO->GetNum();

		if (Settings->bOutputNormalizedIndex)
		{
			DoubleWriter = PointDataFacade->GetWriter<double>(Settings->OutputAttributeName, -1, false, false);
		}
		else
		{
			IntWriter = PointDataFacade->GetWriter<int32>(Settings->OutputAttributeName, -1, false, false);
		}

		if(Settings->bOutputCollectionIndex)
		{
			PCGExData::WriteMark(PointIO->GetOut()->Metadata, Settings->CollectionIndexAttributeName, BatchIndex);
		}
		
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (DoubleWriter) { DoubleWriter->Values[Index] = static_cast<double>(Index) / NumPoints; }
		else { IntWriter->Values[Index] = Index; }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
