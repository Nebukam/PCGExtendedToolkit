// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"

PCGExIO::EInitMode UPCGExWriteIndexSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::DuplicateInput; }

FPCGElementPtr UPCGExWriteIndexSettings::CreateElement() const { return MakeShared<FPCGExWriteIndexElement>(); }

FPCGContext* FPCGExWriteIndexElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExWriteIndexContext* Context = new FPCGExWriteIndexContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExWriteIndexElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }

	FPCGExWriteIndexContext* Context = static_cast<FPCGExWriteIndexContext*>(InContext);

	const UPCGExWriteIndexSettings* Settings = Context->GetInputSettings<UPCGExWriteIndexSettings>();
	check(Settings);

	const FName OutName = Settings->OutputAttributeName;
	if (!FPCGMetadataAttributeBase::IsValidName(OutName))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidName", "Output name is invalid."));
		return false;
	}

	Context->OutName = Settings->OutputAttributeName;
	return true;
	
}


bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	FPCGExWriteIndexContext* Context = static_cast<FPCGExWriteIndexContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		Context->SetState(PCGExMT::EState::ProcessingPoints);
	}

	auto InitializeForIO = [&](UPCGExPointIO* PointIO)
	{
		FWriteScopeLock WriteLock(Context->MapLock);
		PointIO->BuildMetadataEntries();
		FPCGMetadataAttribute<int64>* IndexAttribute = PointIO->Out->Metadata->FindOrCreateAttribute<int64>(Context->OutName, -1, false);
		Context->AttributeMap.Add(PointIO, IndexAttribute);
	};

	auto ProcessPoint = [&](const int32 Index, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& Point = PointIO->GetOutPoint(Index);
		FPCGMetadataAttribute<int64>* IndexAttribute = *(Context->AttributeMap.Find(PointIO));
		IndexAttribute->SetValue(Point.MetadataEntry, Index);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if(Context->AsyncProcessingMainPoints(InitializeForIO, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		/*
		if (Context->MainPoints->OutputsParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		*/
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
