// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWriteTangents.h"

#include "Paths/Tangents/PCGExZeroTangents.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTangentsElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(WriteTangents)

UPCGExWriteTangentsSettings::UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	if (ArriveScaleAttribute.GetName() == FName("@Last")) { ArriveScaleAttribute.Update(TEXT("$Scale")); }
	if (LeaveScaleAttribute.GetName() == FName("@Last")) { LeaveScaleAttribute.Update(TEXT("$Scale")); }
#endif
}

FPCGExWriteTangentsContext::~FPCGExWriteTangentsContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExWriteTangentsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteTangents)

	PCGEX_VALIDATE_NAME(Settings->ArriveName)
	PCGEX_VALIDATE_NAME(Settings->LeaveName)

	PCGEX_OPERATION_BIND(Tangents, UPCGExTangentsOperation)
	Context->Tangents->bClosedPath = Settings->bClosedPath;

	if (Settings->StartTangents)
	{
		Context->StartTangents = Context->RegisterOperation<UPCGExTangentsOperation>(Settings->StartTangents);
		Context->StartTangents->bClosedPath = Settings->bClosedPath;
	}

	if (Settings->EndTangents)
	{
		Context->EndTangents = Context->RegisterOperation<UPCGExTangentsOperation>(Settings->EndTangents);
		Context->EndTangents->bClosedPath = Settings->bClosedPath;
	}

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
		if (LocalTypedContext->StartTangents) { PCGEX_DELETE_OPERATION(StartTangents) }
		if (LocalTypedContext->EndTangents) { PCGEX_DELETE_OPERATION(EndTangents) }
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteTangents)

		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		bClosedPath = Settings->bClosedPath;

		Tangents = Cast<UPCGExTangentsOperation>(PrimaryOperation);
		Tangents->PrepareForData();

		ConstantArriveScale = FVector(Settings->ArriveScaleConstant);
		ConstantLeaveScale = FVector(Settings->LeaveScaleConstant);

		if (Settings->ArriveScaleType == EPCGExFetchType::Attribute)
		{
			ArriveScaleReader = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->ArriveScaleAttribute);
			if (!ArriveScaleReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Invalid Arrive Scale attribute"));
				return false;
			}
		}

		if (Settings->LeaveScaleType == EPCGExFetchType::Attribute)
		{
			LeaveScaleReader = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LeaveScaleAttribute);
			if (!LeaveScaleReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Invalid Arrive Scale attribute"));
				return false;
			}
		}

		if (TypedContext->StartTangents)
		{
			StartTangents = TypedContext->StartTangents->CopyOperation<UPCGExTangentsOperation>();
			StartTangents->PrimaryDataFacade = PointDataFacade;
			StartTangents->PrepareForData();
		}
		else { StartTangents = Tangents; }

		if (TypedContext->EndTangents)
		{
			EndTangents = TypedContext->EndTangents->CopyOperation<UPCGExTangentsOperation>();
			EndTangents->PrimaryDataFacade = PointDataFacade;
			EndTangents->PrepareForData();
		}
		else { EndTangents = Tangents; }

		ArriveWriter = PointDataFacade->GetWriter(Settings->ArriveName, FVector::ZeroVector, true, false);
		LeaveWriter = PointDataFacade->GetWriter(Settings->LeaveName, FVector::ZeroVector, true, false);

		LastIndex = PointIO->GetNum() - 1;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		int32 PrevIndex = Index - 1;
		int32 NextIndex = Index + 1;

		FVector OutArrive = FVector::ZeroVector;
		FVector OutLeave = FVector::ZeroVector;

		const FVector& ArriveScale = ArriveScaleReader ? ArriveScaleReader->Values[Index] : ConstantArriveScale;
		const FVector& LeaveScale = LeaveScaleReader ? LeaveScaleReader->Values[Index] : ConstantLeaveScale;

		if (bClosedPath)
		{
			if (PrevIndex < 0) { PrevIndex = LastIndex; }
			if (NextIndex > LastIndex) { NextIndex = 0; }

			Tangents->ProcessPoint(PointIO->GetIn()->GetPoints(), Index, NextIndex, PrevIndex, ArriveScale, OutArrive, LeaveScale, OutLeave);
		}
		else
		{
			if (Index == 0)
			{
				StartTangents->ProcessFirstPoint(PointIO->GetIn()->GetPoints(), ArriveScale, OutArrive, LeaveScale, OutLeave);
			}
			else if (Index == LastIndex)
			{
				EndTangents->ProcessLastPoint(PointIO->GetIn()->GetPoints(), ArriveScale, OutArrive, LeaveScale, OutLeave);
			}
			else
			{
				Tangents->ProcessPoint(PointIO->GetIn()->GetPoints(), Index, NextIndex, PrevIndex, ArriveScale, OutArrive, LeaveScale, OutLeave);
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
