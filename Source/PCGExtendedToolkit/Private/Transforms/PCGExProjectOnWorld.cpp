// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExProjectOnWorld.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#define LOCTEXT_NAMESPACE "PCGExProjectOnWorldElement"

PCGEx::EIOInit UPCGExProjectOnWorldSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExProjectOnWorldSettings::CreateElement() const { return MakeShared<FPCGExProjectOnWorldElement>(); }

FPCGContext* FPCGExProjectOnWorldElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExProjectOnWorldContext* Context = new FPCGExProjectOnWorldContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}


bool FPCGExProjectOnWorldElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExProjectOnWorldElement::Execute);

	FPCGExProjectOnWorldContext* Context = static_cast<FPCGExProjectOnWorldContext*>(InContext);

	if (Context->IsState(PCGExMT::EState::Setup))
	{
		if (!Context->IsValid())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidContext", "Inputs are missing or invalid."));
			return true;
		}

		const UPCGExProjectOnWorldSettings* Settings = Context->GetInputSettings<UPCGExProjectOnWorldSettings>();
		check(Settings);

		FName OutName = Settings->OutputAttributeName;
		if (!PCGEx::Common::IsValidName(OutName))
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidName", "Output name is invalid."));
			return true;
		}

		Context->OutName = Settings->OutputAttributeName;
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		Context->SetState(PCGExMT::EState::ProcessingPoints);
	}

	auto InitializeForIO = [&Context](UPCGExPointIO* IO)
	{
		FWriteScopeLock ScopeLock(Context->MapLock);
		IO->BuildMetadataEntries();
		FPCGMetadataAttribute<int64>* IndexAttribute = IO->Out->Metadata->FindOrCreateAttribute<int64>(Context->OutName, -1, false, true, true);
		Context->AttributeMap.Add(IO, IndexAttribute);
	};

	auto ProcessPoint = [&Context](const FPCGPoint& Point, const int32 Index, UPCGExPointIO* IO)
	{
		FPCGMetadataAttribute<int64>* IndexAttribute = *(Context->AttributeMap.Find(IO));
		IndexAttribute->SetValue(Point.MetadataEntry, Index);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->Points->OutputsParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::Done);
		}
	}

	if (Context->IsDone())
	{
		Context->Points->OutputTo(Context);
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
