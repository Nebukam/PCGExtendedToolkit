// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExConsolidateRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExConsolidateRelations"

int32 UPCGExConsolidateRelationsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExConsolidateRelationsSettings::GetPointOutputInitMode() const{ return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExConsolidateRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExConsolidateRelationsElement>();
}

FPCGContext* FPCGExConsolidateRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExConsolidateRelationsContext* Context = new FPCGExConsolidateRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExConsolidateRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExConsolidateRelationsContext* Context = static_cast<FPCGExConsolidateRelationsContext*>(InContext);
	// ...
}

bool FPCGExConsolidateRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConsolidateRelationsElement::Execute);

	FPCGExConsolidateRelationsContext* Context = static_cast<FPCGExConsolidateRelationsContext*>(InContext);

	if (Context->IsCurrentOperation(PCGEx::EOperation::Setup))
	{
		if (Context->Params.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
			return true;
		}

		if (Context->Points->IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
			return true;
		}

		Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);

#if WITH_EDITOR
		const UPCGExConsolidateRelationsSettings* Settings = Context->GetInputSettings<UPCGExConsolidateRelationsSettings>();
		check(Settings);

		if (Settings->bDebug)
		{
			if (const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				FlushPersistentDebugLines(EditorWorld);
			}
		}
#endif
	}
	
	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			Context->SetOperation(PCGEx::EOperation::ProcessingPoints);
		}
	}

	auto InitializeDelta = [&Context](UPCGExPointIO* IO)
	{
		Context->Deltas.Empty();
		IO->BuildMetadataEntries();		
	};

	auto CapturePointDelta = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		FWriteScopeLock ScopeLock(Context->DeltaLock);
		Context->Deltas.Add(Context->CachedIndex->GetValueFromItemKey(Point.MetadataEntry), ReadIndex); // Cache previous
		Context->CachedIndex->SetValue(Point.MetadataEntry, ReadIndex); // Update with current
	};
	
	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingPoints))
	{
		if(Context->CurrentIO->OutputParallelProcessing(Context, InitializeDelta, CapturePointDelta, 256))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetOperation(PCGEx::EOperation::ProcessingPoints2ndPass);
		}
	}
	
	auto InitializeForParams = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};
	
	auto ConsolidatePoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		// TODO: Rebuilding indices 
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingPoints2ndPass))
	{
		if(Context->CurrentIO->OutputParallelProcessing(Context, InitializeForParams, ConsolidatePoint, Context->ChunkSize))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Points->OutputTo(Context);
		return true;
	}

	return false;
}

#if WITH_EDITOR
void FPCGExConsolidateRelationsElement::DrawRelationsDebug(FPCGExConsolidateRelationsContext* Context) const
{

	const UPCGExConsolidateRelationsSettings* Settings = Context->GetInputSettings<UPCGExConsolidateRelationsSettings>();

	if (!Context->CurrentParams || !Context->CurrentIO || !Settings->bDebug) { return; }
	
	if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
	{
		auto DrawDebug = [&Context, &Settings, &EditorWorld](int32 ReadIndex)
		{
			FPCGPoint PtA = Context->CurrentIO->Out->GetPoint(ReadIndex);
			int64 Key = PtA.MetadataEntry;

			FVector Start = PtA.Transform.GetLocation();
			for (const PCGExRelational::FSocket& Socket : Context->CurrentParams->GetSocketMapping()->Sockets)
			{
				PCGExRelational::FSocketMetadata SocketMetadata = Socket.GetData(Key);
				if (SocketMetadata.Index == -1) { continue; }

				FPCGPoint PtB = Context->CurrentIO->Out->GetPoint(SocketMetadata.Index);
				FVector End = FMath::Lerp(Start, PtB.Transform.GetLocation(), 0.4);
				DrawDebugDirectionalArrow(EditorWorld, Start, End, 2.0f, Socket.Descriptor.DebugColor, false, Settings->DebugDrawLifetime, 0, 2);
			}
		};

		//PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, DrawDebug);
		for (int i = 0; i < Context->CurrentIO->NumPoints; i++) { DrawDebug(i); }
	}
}
#endif

#undef LOCTEXT_NAMESPACE
