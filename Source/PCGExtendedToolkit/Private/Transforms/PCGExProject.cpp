// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transforms/PCGExProject.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGExProjectElement"

PCGEx::EIOInit UPCGExProjectSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExProjectSettings::CreateElement() const { return MakeShared<FPCGExProjectElement>(); }

FPCGContext* FPCGExProjectElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExProjectContext* Context = new FPCGExProjectContext();
	InitializeContext(Context, InputData, SourceComponent, Node);

	const UPCGExProjectSettings* Settings = Context->GetInputSettings<UPCGExProjectSettings>();
	check(Settings);

	Context->AttemptStepSize = FMath::Max(Settings->MaxDistance / static_cast<double>(Settings->NumMaxAttempts), Settings->MinStepSize);
	Context->NumMaxAttempts = FMath::Max(static_cast<int32>(static_cast<double>(Settings->MaxDistance) / Context->AttemptStepSize), 1);
	Context->CollisionChannel = Settings->CollisionChannel;
	Context->bIgnoreSelf = Settings->bIgnoreSelf;

	return Context;
}

bool FPCGExProjectElement::Validate(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Validate(InContext)) { return false; }
	return true;
}

bool FPCGExProjectElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExProjectElement::Execute);

	FPCGExProjectContext* Context = static_cast<FPCGExProjectContext*>(InContext);

	if (Context->IsState(PCGExMT::EState::Setup))
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

	auto InitializeForIO = [&Context](UPCGExPointIO* IO)
	{
		Context->NumSweepComplete = 0;
		IO->BuildMetadataEntries();
		//Context->HitLocationAttribute = IO->Out->Metadata->FindOrCreateAttribute<FVector>(Context->OutName, FVector::ZeroVector, false, true, true);
	};

	auto ProcessPoint = [&Context, this](const FPCGPoint& Point, const int32 Index, UPCGExPointIO* IO)
	{
		Context->ScheduleTask<PCGExAsync::FSweepSphereTask>(Index, Point.MetadataEntry);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForIO, ProcessPoint, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::WaitingOnAsyncTasks);
		}
	}

	if (Context->IsState(PCGExMT::EState::WaitingOnAsyncTasks))
	{
		if (Context->NumSweepComplete == Context->CurrentIO->NumPoints)
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
