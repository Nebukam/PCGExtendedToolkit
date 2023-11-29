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
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

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

		if (NextPtr && PrevPtr)
		{
			const FVector PrevPos = PrevPtr->Transform.GetLocation();
			const FVector NextPos = NextPtr->Transform.GetLocation();
			const FVector MidPos = MidPoint.Transform.GetLocation();

			const FVector Dir = (NextPos - PrevPos).GetSafeNormal();
			const FVector Anchor = FMath::ClosestPointOnSegment(MidPos, PrevPos, NextPos);

			const FVector ArriveTangent = Dir * (FVector::Dist(PrevPos, Anchor)) * Context->Scale;
			const FVector LeaveTangent = Dir * (FVector::Dist(NextPos, Anchor)) * Context->Scale;

			Context->ArriveAttribute->SetValue(MidPoint.MetadataEntry, ArriveTangent);
			Context->LeaveAttribute->SetValue(MidPoint.MetadataEntry, LeaveTangent);
		}
		else
		{
			if (NextPtr)
			{
				// First point
			}
			else if (PrevPtr)
			{
				// Last point
			}
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
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
