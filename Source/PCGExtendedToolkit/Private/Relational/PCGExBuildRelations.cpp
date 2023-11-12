// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExBuildRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExBuildRelations"

#if WITH_EDITOR
FText UPCGExBuildRelationsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

int32 UPCGExBuildRelationsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExBuildRelationsSettings::GetPointOutputInitMode() const{ return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExBuildRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExBuildRelationsElement>();
}

FPCGContext* FPCGExBuildRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExBuildRelationsContext* Context = new FPCGExBuildRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExBuildRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	FPCGExBuildRelationsContext* Context = static_cast<FPCGExBuildRelationsContext*>(InContext);
	// ...
}

bool FPCGExBuildRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildRelationsElement::Execute);

	FPCGExBuildRelationsContext* Context = static_cast<FPCGExBuildRelationsContext*>(InContext);

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
		const UPCGExBuildRelationsSettings* Settings = Context->GetInputSettings<UPCGExBuildRelationsSettings>();
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
		if (Context->CurrentIO)
		{
			//Cleanup current IO, indices won't be needed anymore.
			Context->CurrentIO->Flush();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(PCGEx::EOperation::Done); //No more points
		}
		else
		{
			Context->CurrentIO->BuildMetadataEntriesAndIndices();
			Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->Out->GetOctree())); // Not sure this really saves perf
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{

		TArray<PCGExRelational::FSocketCandidate> Candidates;
		const double MaxDistance = PCGExRelational::Helpers::PrepareCandidatesForPoint(Point, Context->CurrentParams, Candidates);

		auto ProcessPointNeighbor = [&ReadIndex, &Candidates, &IO](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = IO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == ReadIndex) { return; }

			for (PCGExRelational::FSocketCandidate& SocketCandidate : Candidates)
			{
				if (SocketCandidate.ProcessPoint(OtherPoint)) { SocketCandidate.Index = Index; }
			}
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		//Write results
		PCGMetadataEntryKey Key = Point.MetadataEntry;
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
		const UPCGExBuildRelationsSettings* Settings = Context->GetInputSettings<UPCGExBuildRelationsSettings>();
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

	auto Initialize = [&Context](UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(IO->Out);
		Context->SetOperation(PCGEx::EOperation::ProcessingParams);
	};

	if (Context->IsCurrentOperation(PCGEx::EOperation::ProcessingParams) || bProcessingAllowed)
	{
		if(Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
		/*
		if (PCGEx::Common::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, ProcessPoint, Context->ChunkSize))
		{
			Context->SetOperation(PCGEx::EOperation::ReadyForNextParams);
		}
		*/
	}

	if (Context->IsCurrentOperation(PCGEx::EOperation::Done))
	{
		Context->Points->OutputTo(Context);
		return true;
	}

	return false;
}

#if WITH_EDITOR
void FPCGExBuildRelationsElement::DrawRelationsDebug(FPCGExBuildRelationsContext* Context) const
{
	if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
	{
		FlushPersistentDebugLines(EditorWorld);
		Context->CurrentParams->PrepareForPointData(Context->CurrentIO->Out);
		auto DrawDebug = [&Context, &EditorWorld](int32 ReadIndex)
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
				DrawDebugDirectionalArrow(EditorWorld, Start, End, 2.0f, Socket.Descriptor.DebugColor, true, -1, 0, 2);
			}
		};

		//PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, DrawDebug);
		for (int i = 0; i < Context->CurrentIO->NumPoints; i++) { DrawDebug(i); }
	}
}
#endif

#undef LOCTEXT_NAMESPACE
