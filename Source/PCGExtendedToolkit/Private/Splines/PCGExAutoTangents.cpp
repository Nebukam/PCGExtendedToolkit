// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExAutoTangents.h"

#define LOCTEXT_NAMESPACE "PCGExAutoTangentsElement"

PCGExIO::EInitMode UPCGExAutoTangentsSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

FPCGElementPtr UPCGExAutoTangentsSettings::CreateElement() const { return MakeShared<FPCGExAutoTangentsElement>(); }

FPCGContext* FPCGExAutoTangentsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExAutoTangentsContext* Context = new FPCGExAutoTangentsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExAutoTangentsSettings* Settings = Context->GetInputSettings<UPCGExAutoTangentsSettings>();
	check(Settings);

	Context->ArriveName = Settings->ArriveName;
	Context->LeaveName = Settings->LeaveName;
	Context->ScaleMode = Settings->ScaleMode;
	Context->Scale = Settings->Scale;

	return Context;
}


bool FPCGExAutoTangentsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAutoTangentsElement::Execute);

	FPCGExAutoTangentsContext* Context = static_cast<FPCGExAutoTangentsContext*>(InContext);

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
		auto Initialize = [&](UPCGExPointIO* PointIO)
		{
			PointIO->BuildMetadataEntries();
			Context->ArriveAttribute = PointIO->Out->Metadata->FindOrCreateAttribute(Context->ArriveName, FVector::ZeroVector);
			Context->LeaveAttribute = PointIO->Out->Metadata->FindOrCreateAttribute(Context->LeaveName, FVector::ZeroVector);
		};

		auto ProcessPoint = [&](const int32 Index, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& MidPoint = PointIO->GetOutPoint(Index);
			const FPCGPoint* PrevPtr = PointIO->TryGetOutPoint(Index - 1);
			const FPCGPoint* NextPtr = PointIO->TryGetOutPoint(Index + 1);
			PCGExMath::FApex Apex;

			if (NextPtr && PrevPtr)
			{
				Apex = PCGExMath::FApex(
					PrevPtr->Transform.GetLocation(),
					NextPtr->Transform.GetLocation(),
					MidPoint.Transform.GetLocation());
			}
			else
			{
				if (NextPtr) // First point
				{
					Apex = PCGExMath::FApex::FromB(NextPtr->Transform.GetLocation(), MidPoint.Transform.GetLocation());
				}
				else if (PrevPtr) // Last point
				{
					Apex = PCGExMath::FApex::FromA(PrevPtr->Transform.GetLocation(), MidPoint.Transform.GetLocation());
				}
			}

			Apex.Scale(Context->Scale);

			Context->ArriveAttribute->SetValue(MidPoint.MetadataEntry, Apex.Forward);
			Context->LeaveAttribute->SetValue(MidPoint.MetadataEntry, Apex.Backward * -1);
		};

		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
