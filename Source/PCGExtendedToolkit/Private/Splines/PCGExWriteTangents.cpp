// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExWriteTangents.h"

#include "Splines/Tangents/PCGExAutoTangents.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTangentsElement"
#define PCGEX_NAMESPACE BuildGraph

UPCGExWriteTangentsSettings::UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Tangents = EnsureOperation<UPCGExAutoTangents>(Tangents);
}

void UPCGExWriteTangentsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Tangents = EnsureOperation<UPCGExAutoTangents>(Tangents);
	if (Tangents) { Tangents->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExWriteTangentsSettings::CreateElement() const { return MakeShared<FPCGExWriteTangentsElement>(); }

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

PCGEX_INITIALIZE_CONTEXT(WriteTangents)

bool FPCGExWriteTangentsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteTangents)

	PCGEX_BIND_OPERATION(Tangents, UPCGExAutoTangents)
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

	PCGEX_CONTEXT(WriteTangents)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			const int32 NumPoints = PointIO.GetNum();
			Context->ArriveTangents.SetNum(NumPoints);
			Context->LeaveTangents.SetNum(NumPoints);

			Context->ArriveTangentsAccessor = PCGEx::FAttributeAccessor<FVector>::FindOrCreate(PointIO, Context->Tangents->ArriveName);
			Context->LeaveTangentsAccessor = PCGEx::FAttributeAccessor<FVector>::FindOrCreate(PointIO, Context->Tangents->LeaveName);

			Context->Tangents->PrepareForData(PointIO);
		};

		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			FVector& OutArrive = Context->ArriveTangents[Index];
			FVector& OutLeave = Context->LeaveTangents[Index];

			const PCGEx::FPointRef MainPoint = PCGEx::FPointRef(PointIO.GetOutPoint(Index), Index);
			const PCGEx::FPointRef PrevPoint = PCGEx::FPointRef(PointIO.TryGetOutPoint(Index - 1), Index - 1);
			const PCGEx::FPointRef NextPoint = PCGEx::FPointRef(PointIO.TryGetOutPoint(Index + 1), Index + 1);

			if (NextPoint.IsValid() && PrevPoint.IsValid()) { Context->Tangents->ProcessPoint(MainPoint, PrevPoint, NextPoint, OutArrive, OutLeave); }
			else if (NextPoint.IsValid()) { Context->Tangents->ProcessFirstPoint(MainPoint, NextPoint, OutArrive, OutLeave); }
			else if (PrevPoint.IsValid()) { Context->Tangents->ProcessLastPoint(MainPoint, PrevPoint, OutArrive, OutLeave); }
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->WriteTangents();
			Context->CurrentIO->OutputTo(Context);
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
