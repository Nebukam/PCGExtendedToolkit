// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBitmask.h"
#include "Misc/PCGExBitmaskOperation.h"

#define LOCTEXT_NAMESPACE "PCGExBitmaskOperationElement"
#define PCGEX_NAMESPACE BitmaskOperation

PCGExData::EInit UPCGExBitmaskOperationSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(BitmaskOperation)

FPCGExBitmaskOperationContext::~FPCGExBitmaskOperationContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExBitmaskOperationElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BitmaskOperation)

	PCGEX_VALIDATE_NAME(Settings->FlagAttribute)

	if (Settings->MaskType == EPCGExFetchType::Attribute)
	{
		PCGEX_VALIDATE_NAME(Settings->MaskAttribute)
	}

	return true;
}

bool FPCGExBitmaskOperationElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBitmaskOperationElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BitmaskOperation)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBitmaskOperation::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Settings->MaskType == EPCGExFetchType::Attribute && !Entry->GetOut()->Metadata->HasAttribute(Settings->MaskAttribute))
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBitmaskOperation::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any points to process."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs are missing the specified MaskAttribute and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExBitmaskOperation
{
	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BitmaskOperation)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		Writer = PointDataFacade->GetOrCreateWriter<int64>(Settings->FlagAttribute, 0, false, false);

		if (Settings->MaskType == EPCGExFetchType::Attribute)
		{
			Reader = PointDataFacade->GetOrCreateReader<int64>(Settings->MaskAttribute);
			if (!Reader) { return false; }
		}
		else
		{
			Mask = Settings->BitMask.Get();
		}

		Op = Settings->Operation;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		PCGExBitmask::Do(Op, Writer->Values[Index], Reader ? Reader->Values[Index] : Mask);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
