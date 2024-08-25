// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWriteTangents.h"

#include "Paths/Tangents/PCGExZeroTangents.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTangentsElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(WriteTangents)

FPCGExWriteTangentsContext::~FPCGExWriteTangentsContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWriteTangentsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteTangents)

	PCGEX_OPERATION_BIND(Tangents, UPCGExZeroTangents)

	Context->Tangents->bClosedPath = Settings->bClosedPath;

	PCGEX_VALIDATE_NAME(Settings->ArriveName)
	PCGEX_VALIDATE_NAME(Settings->LeaveName)

	return true;
}


bool FPCGExWriteTangentsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteTangentsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteTangents)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWriteTangents::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExWriteTangents::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Tangents;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to write tangents to."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExWriteTangents
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteTangents)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;

		Tangents = Cast<UPCGExTangentsOperation>(PrimaryOperation);
		Tangents->PrepareForData(PointDataFacade);

		ArriveWriter = PointDataFacade->GetWriter(Settings->ArriveName, FVector::ZeroVector, true, false);
		LeaveWriter = PointDataFacade->GetWriter(Settings->LeaveName, FVector::ZeroVector, true, false);

		LastIndex = PointIO->GetNum() - 1;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		int32 PrevIndex = Index - 1;
		int32 NextIndex = Index + 1;

		FVector OutArrive = FVector::ZeroVector;
		FVector OutLeave = FVector::ZeroVector;

		if (bClosedPath)
		{
			if (PrevIndex < 0) { PrevIndex = LastIndex; }
			if (NextIndex > LastIndex) { NextIndex = 0; }

			Tangents->ProcessPoint(PointIO->GetIn()->GetPoints(), Index, NextIndex, PrevIndex, OutArrive, OutLeave);
		}
		else
		{
			if (PrevIndex >= 0 && NextIndex <= LastIndex)
			{
				Tangents->ProcessPoint(PointIO->GetIn()->GetPoints(), Index, NextIndex, PrevIndex, OutArrive, OutLeave);
			}
			else if (PrevIndex < 0)
			{
				Tangents->ProcessFirstPoint(PointIO->GetIn()->GetPoints(), OutArrive, OutLeave);
			}
			else if (NextIndex > LastIndex)
			{
				Tangents->ProcessLastPoint(PointIO->GetIn()->GetPoints(), OutArrive, OutLeave);
			}
		}

		ArriveWriter->Values[Index] = OutArrive;
		LeaveWriter->Values[Index] = OutLeave;
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
