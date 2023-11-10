// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExMarkMutualRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"
#include <mutex>

#define LOCTEXT_NAMESPACE "PCGExMarkMutualRelations"

#if WITH_EDITOR
FText UPCGExMarkMutualRelationsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExMarkMutualRelationsTooltip", "Process existing relations to find and mark shared (mutual) connections.");
}
#endif // WITH_EDITOR

int32 UPCGExMarkMutualRelationsSettings::GetPreferredChunkSize() const { return 32; }

FPCGElementPtr UPCGExMarkMutualRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExMarkMutualRelationsElement>();
}

FPCGContext* FPCGExMarkMutualRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExMarkMutualRelationsContext* Context = new FPCGExMarkMutualRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExMarkMutualRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	FPCGExMarkMutualRelationsContext* Context = static_cast<FPCGExMarkMutualRelationsContext*>(InContext);
	// ...
}

bool FPCGExMarkMutualRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExMarkMutualRelationsElement::Execute);

	FPCGExMarkMutualRelationsContext* Context = static_cast<FPCGExMarkMutualRelationsContext*>(InContext);

	if (Context->IsCurrentOperation(PCGEx::EOperation::Setup))
	{
		if (Context->Params.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing Input Params."));
			return true;
		}

		if (Context->Points.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingPoints", "Missing Input Points."));
			return true;
		}

		Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextPoints))
	{
		if (Context->CurrentIO)
		{
			//Cleanup current IO, indices won't be needed anymore.
			Context->CurrentIO->IndicesMap.Empty();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			Context->CurrentIO->ForwardPoints(Context, true);
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context](
		int32 ReadIndex)
	{
		FPCGPoint InPoint = Context->CurrentIO->In->GetPoint(ReadIndex),
		          OutPoint = Context->CurrentIO->Out->GetPoint(ReadIndex);

		TArray<PCGExRelational::FSocketCandidate> Candidates;
		const double MaxDistance = PCGExRelational::Helpers::PrepareCandidatesForPoint(InPoint, Context->CurrentParams, Candidates);

		auto ProcessPointNeighbor = [&Context, &ReadIndex, &Candidates](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = Context->CurrentIO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == ReadIndex) { return; }

			for (PCGExRelational::FSocketCandidate& SocketCandidate : Candidates)
			{
				if (SocketCandidate.ProcessPoint(OtherPoint)) { SocketCandidate.Index = Index; }
			}
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(OutPoint.Transform.GetLocation(), FVector(MaxDistance));

		//UE_LOG(LogTemp, Warning, TEXT("           Processed Point %d"), ReadIndex);

		//Write results
		int64 Key = OutPoint.MetadataEntry;
		for (int i = 0; i < Candidates.Num(); i++)
		{
			const PCGExRelational::FSocket* Socket = &(Context->CurrentParams->GetSocketMapping()->Sockets[i]);
			Socket->SetValue(Key, Candidates[i].ToSocketData());
		}
	};

	bool bProcessingAllowed = false;

	if (Context->IsCurrentOperation(PCGEx::EOperation::ReadyForNextParams))
	{
#if WITH_EDITOR
		const UPCGExMarkMutualRelationsSettings* Settings = Context->GetInputSettings<UPCGExMarkMutualRelationsSettings>();
		check(Settings);

		if (Context->CurrentParams && Settings->bDebug) { DrawRelationsDebug(Context); }
#endif

		if (!Context->AdvanceParams())
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextPoints);
			return false;
		}
		else
		{
			bProcessingAllowed = true;
		}
	}

	auto Initialize = [&Context]()
	{
		Context->CurrentParams->PrepareForPointData(Context->CurrentIO->Out);
		Context->SetOperation(PCGEx::EOperation::ProcessingParams);
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingParams) || bProcessingAllowed)
	{
		if (PCGEx::Common::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);

		}
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Points.OutputTo(Context);
		return true;
	}

	return false;
}

void FPCGExMarkMutualRelationsElement::DrawRelationsDebug(FPCGExMarkMutualRelationsContext* Context) const
{
#if WITH_EDITOR
	if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
	{
		Context->CurrentParams->PrepareForPointData(Context->CurrentIO->Out);
		auto DrawDebug = [&Context, &EditorWorld](
			int32 ReadIndex)
		{
			FPCGPoint PtA = Context->CurrentIO->Out->GetPoint(ReadIndex);
			int64 Key = PtA.MetadataEntry;

			FVector Start = PtA.Transform.GetLocation();
			for (const PCGExRelational::FSocket& Socket : Context->CurrentParams->GetSocketMapping()->Sockets)
			{
				PCGExRelational::FSocketData SocketData = Socket.GetSocketData(Key);
				if (SocketData.Index == -1) { continue; }

				FPCGPoint PtB = Context->CurrentIO->Out->GetPoint(SocketData.Index);
				FVector End = FMath::Lerp(Start, PtB.Transform.GetLocation(), 0.4);
				DrawDebugLine(EditorWorld, Start, End, Socket.Descriptor.DebugColor, false, 10.0f, 0, 2);
			}
		};

		PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, DrawDebug);
	}
#endif
}

#undef LOCTEXT_NAMESPACE
