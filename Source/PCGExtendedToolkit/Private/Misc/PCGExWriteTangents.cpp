// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteTangents.h"

#define LOCTEXT_NAMESPACE "PCGExWriteTangentsElement"

PCGExIO::EInitMode UPCGExWriteTangentsSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

FPCGElementPtr UPCGExWriteTangentsSettings::CreateElement() const { return MakeShared<FPCGExWriteTangentsElement>(); }

FPCGContext* FPCGExWriteTangentsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExWriteTangentsContext* Context = new FPCGExWriteTangentsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExWriteTangentsSettings* Settings = Context->GetInputSettings<UPCGExWriteTangentsSettings>();
	check(Settings);

	Context->TangentParams = Settings->TangentParams;
	Context->CurvePointMode = Settings->CurveMode;
	return Context;
}


bool FPCGExWriteTangentsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteTangentsElement::Execute);

	FPCGExWriteTangentsContext* Context = static_cast<FPCGExWriteTangentsContext*>(InContext);

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

		if (Context->CurvePointMode == EPCGExCurvePointMode::Relational)
		{
			Context->TangentCache.Empty();
			Context->TangentCache.Reserve(PointIO->NumInPoints);
		}

		Context->TangentParams.PrepareForData(PointIO);
	};

	auto ProcessPoint = [&](const int32 Index, const UPCGExPointIO* PointIO)
	{
		if (Context->CurvePointMode == EPCGExCurvePointMode::Relational)
		{
			FWriteScopeLock WriteLock(Context->MapLock);
			Context->TangentParams.ComputePointTangents(Index, PointIO, &Context->TangentCache);
		}
		else
		{
			Context->TangentParams.ComputePointTangents(Index, PointIO);
		}
	};

	auto ProcessRelationalTangents = [&](const int32 Index, const UPCGExPointIO* PointIO)
	{
		FReadScopeLock ReadLock(Context->MapLock);
		Context->TangentParams.ComputeRelationalTangents(Index, PointIO, &Context->TangentCache);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->AsyncProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			if (Context->CurvePointMode == EPCGExCurvePointMode::Relational)
			{
				Context->SetState(PCGExMT::EState::ProcessingPoints2ndPass);
			}
			else
			{
				Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			}
		}
	}

	if (Context->IsState(PCGExMT::EState::ProcessingPoints2ndPass))
	{
		if (Context->AsyncProcessingCurrentPoints(ProcessRelationalTangents))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->TangentCache.Empty();
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
