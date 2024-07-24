// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWriteTangents.h"

#include "Paths/Tangents/PCGExAutoTangents.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTangentsElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(WriteTangents)

FPCGExWriteTangentsContext::~FPCGExWriteTangentsContext()
{
	Tangents = nullptr;
	ArriveTangents.Empty();
	LeaveTangents.Empty();
	PCGEX_DELETE(ArriveTangentsAccessor)
	PCGEX_DELETE(LeaveTangentsAccessor)
}

void FPCGExWriteTangentsContext::WriteTangents()
{
	ArriveTangentsAccessor->SetRange(ArriveTangents, 0, CurrentIO->GetOutKeys());
	LeaveTangentsAccessor->SetRange(LeaveTangents, 0, CurrentIO->GetOutKeys());

	PCGEX_DELETE(ArriveTangentsAccessor)
	PCGEX_DELETE(LeaveTangentsAccessor)
}


bool FPCGExWriteTangentsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteTangents)

	PCGEX_OPERATION_BIND(Tangents, UPCGExAutoTangents)

	Context->Tangents->ArriveName = Settings->ArriveName;
	Context->Tangents->LeaveName = Settings->LeaveName;

	if (!FPCGMetadataAttributeBase::IsValidName(Context->Tangents->ArriveName) ||
		!FPCGMetadataAttributeBase::IsValidName(Context->Tangents->LeaveName))
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Invalid attribute names"));
		return false;
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
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->Done();
		}
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		const int32 NumPoints = Context->CurrentIO->GetNum();

		auto Initialize = [&]()
		{
			Context->ArriveTangents.SetNum(NumPoints);
			Context->LeaveTangents.SetNum(NumPoints);

			Context->ArriveTangentsAccessor = PCGEx::FAttributeAccessor<FVector>::FindOrCreate(Context->CurrentIO, Context->Tangents->ArriveName);
			Context->LeaveTangentsAccessor = PCGEx::FAttributeAccessor<FVector>::FindOrCreate(Context->CurrentIO, Context->Tangents->LeaveName);

			Context->Tangents->PrepareForData(Context->CurrentIO);
		};

		auto ProcessPoint = [&](const int32 Index)
		{
			FVector& OutArrive = Context->ArriveTangents[Index];
			FVector& OutLeave = Context->LeaveTangents[Index];

			const PCGExData::FPointRef MainPoint = PCGExData::FPointRef(Context->CurrentIO->GetOutPoint(Index), Index);
			const PCGExData::FPointRef PrevPoint = PCGExData::FPointRef(Context->CurrentIO->TryGetOutPoint(Index - 1), Index - 1);
			const PCGExData::FPointRef NextPoint = PCGExData::FPointRef(Context->CurrentIO->TryGetOutPoint(Index + 1), Index + 1);

			if (NextPoint.IsValid() && PrevPoint.IsValid()) { Context->Tangents->ProcessPoint(MainPoint, PrevPoint, NextPoint, OutArrive, OutLeave); }
			else if (NextPoint.IsValid()) { Context->Tangents->ProcessFirstPoint(MainPoint, NextPoint, OutArrive, OutLeave); }
			else if (PrevPoint.IsValid()) { Context->Tangents->ProcessLastPoint(MainPoint, PrevPoint, OutArrive, OutLeave); }
		};

		auto ProcessPointTile = [&](const int32 Index)
		{
			FVector& OutArrive = Context->ArriveTangents[Index];
			FVector& OutLeave = Context->LeaveTangents[Index];

			const int32 MaxIndex = NumPoints - 1;
			const int32 PrevIndex = PCGExMath::Tile(Index - 1, 0, MaxIndex);
			const int32 NextIndex = PCGExMath::Tile(Index + 1, 0, MaxIndex);

			const PCGExData::FPointRef MainPoint = PCGExData::FPointRef(Context->CurrentIO->GetOutPoint(Index), Index);
			const PCGExData::FPointRef PrevPoint = PCGExData::FPointRef(Context->CurrentIO->GetOutPoint(PrevIndex), PrevIndex);
			const PCGExData::FPointRef NextPoint = PCGExData::FPointRef(Context->CurrentIO->GetOutPoint(NextIndex), NextIndex);

			Context->Tangents->ProcessPoint(MainPoint, PrevPoint, NextPoint, OutArrive, OutLeave);
		};

		bool bProcessingComplete;
		if (Settings->bClosedPath) { bProcessingComplete = Context->Process(Initialize, ProcessPointTile, NumPoints); }
		else { bProcessingComplete = Context->Process(Initialize, ProcessPoint, NumPoints); }

		if (!bProcessingComplete) { return false; }

		Context->WriteTangents();
		Context->CurrentIO->OutputToContext();
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
