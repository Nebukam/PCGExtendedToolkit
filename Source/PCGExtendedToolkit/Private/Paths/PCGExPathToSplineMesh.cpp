// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathToSplineMesh.h"

#include "Paths/Tangents/PCGExZeroTangents.h"

#define LOCTEXT_NAMESPACE "PCGExPathToSplineMeshElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(PathToSplineMesh)

FPCGExPathToSplineMeshContext::~FPCGExPathToSplineMeshContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExPathToSplineMeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathToSplineMesh)

	if (!Settings->bTangentsFromAttributes) { PCGEX_OPERATION_BIND(Tangents, UPCGExZeroTangents) }

	Context->Tangents->bClosedPath = Settings->bClosedPath;

	return true;
}


bool FPCGExPathToSplineMeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathToSplineMeshElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathToSplineMesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathToSplineMesh::FProcessor>>(
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
			[&](PCGExPointsMT::TBatch<PCGExPathToSplineMesh::FProcessor>* NewBatch)
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

namespace PCGExPathToSplineMesh
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathToSplineMesh)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;

		if (Settings->bTangentsFromAttributes)
		{
			Tangents = Cast<UPCGExTangentsOperation>(PrimaryOperation);
			Tangents->PrepareForData(PointDataFacade);
		}
		else
		{
			ArriveReader = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Arrive);
			LeaveReader = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Leave);
		}

		LastIndex = PointIO->GetNum() - 1;

		PCGEX_SET_NUM_UNINITIALIZED(SplineMeshParams, PointIO->GetNum())

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}

	void FProcessor::Output()
	{
		// Called from main
		// TODO : create/manage component on target actor
		FPointsProcessor::Output();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
