// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExWriteTangents.h"

#include "Splines/Tangents/PCGExAutoTangents.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTangentsElement"

UPCGExWriteTangentsSettings::UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Tangents = EnsureInstruction<UPCGExAutoTangents>(Tangents);
}

void UPCGExWriteTangentsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Tangents = EnsureInstruction<UPCGExAutoTangents>(Tangents);
	if (Tangents) { Tangents->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

FPCGElementPtr UPCGExWriteTangentsSettings::CreateElement() const { return MakeShared<FPCGExWriteTangentsElement>(); }

FPCGContext* FPCGExWriteTangentsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExWriteTangentsContext* Context = new FPCGExWriteTangentsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExWriteTangentsSettings* Settings = Context->GetInputSettings<UPCGExWriteTangentsSettings>();
	check(Settings);

	UPCGExAutoTangents* Tangents = Settings->EnsureInstruction<UPCGExAutoTangents>(Settings->Tangents, Context);
	Tangents->ArriveName = Settings->ArriveName;
	Tangents->LeaveName = Settings->LeaveName;

	Context->Tangents = Tangents;

	return Context;
}


bool FPCGExWriteTangentsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteTangentsElement::Execute);

	FPCGExWriteTangentsContext* Context = static_cast<FPCGExWriteTangentsContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
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
			PointIO.BuildMetadataEntries();
			Context->Tangents->PrepareForData(PointIO);
		};

		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			const FPCGPoint& Point = PointIO.GetOutPoint(Index);
			const FPCGPoint* PrevPtr = PointIO.TryGetOutPoint(Index - 1);
			const FPCGPoint* NextPtr = PointIO.TryGetOutPoint(Index + 1);

			if (NextPtr && PrevPtr) { Context->Tangents->ProcessPoint(Index, Point, *PrevPtr, *NextPtr); }
			else if (NextPtr) { Context->Tangents->ProcessFirstPoint(Index, Point, *NextPtr); }
			else if (PrevPtr) { Context->Tangents->ProcessLastPoint(Index, Point, *PrevPtr); }
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
