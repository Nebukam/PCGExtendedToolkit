// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBitwiseOperation.h"


#include "Misc/PCGExBitmask.h"

#define LOCTEXT_NAMESPACE "PCGExBitwiseOperationElement"
#define PCGEX_NAMESPACE BitwiseOperation

PCGExData::EIOInit UPCGExBitwiseOperationSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(BitwiseOperation)

bool FPCGExBitwiseOperationElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BitwiseOperation)

	PCGEX_VALIDATE_NAME(Settings->FlagAttribute)

	if (Settings->MaskInput == EPCGExInputValueType::Attribute)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->MaskAttribute)
	}

	return true;
}

bool FPCGExBitwiseOperationElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBitwiseOperationElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BitwiseOperation)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs are missing the specified MaskAttribute and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBitwiseOperation::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Settings->MaskInput == EPCGExInputValueType::Attribute && !Entry->GetOut()->Metadata->HasAttribute(Settings->MaskAttribute))
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBitwiseOperation::FProcessor>>& NewBatch)
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

namespace PCGExBitwiseOperation
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBitwiseOperation::Process);


		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Writer = PointDataFacade->GetWritable<int64>(Settings->FlagAttribute, 0, false, PCGExData::EBufferInit::Inherit);

		if (Settings->MaskInput == EPCGExInputValueType::Attribute)
		{
			Reader = PointDataFacade->GetScopedReadable<int64>(Settings->MaskAttribute);
			if (!Reader) { return false; }
		}
		else
		{
			Mask = Settings->Bitmask;
		}

		Op = Settings->Operation;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		PCGExBitmask::Do(Op, Writer->GetMutable(Index), Reader ? Reader->Read(Index) : Mask);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
