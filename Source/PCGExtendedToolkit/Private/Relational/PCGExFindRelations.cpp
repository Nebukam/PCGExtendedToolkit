// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExFindRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExFindRelations"

#if WITH_EDITOR
FText UPCGExFindRelationsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGExFindRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExFindRelationsElement>();
}

FPCGContext* FPCGExFindRelationsElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExFindRelationsContext* Context = InitializeRelationsContext<FPCGExFindRelationsContext>(InputData, SourceComponent, Node);
	return Context;
}

bool FPCGExFindRelationsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindRelationsElement::Execute);

	FPCGExFindRelationsContext* Context = static_cast<FPCGExFindRelationsContext*>(InContext);

	const UPCGExFindRelationsSettings* Settings = Context->GetInputSettings<UPCGExFindRelationsSettings>();
	check(Settings);

	if (Context->IsCurrentOperation(Setup))
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

		Context->SetOperation(ReadyForNextPoints);
	}

	if (Context->IsCurrentOperation(ReadyForNextPoints))
	{
		if (Context->CurrentIO)
		{
			//Cleanup current IO, indices won't be needed anymore.
			Context->CurrentIO->IndicesMap.Empty();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->SetOperation(Done); //No more points
		}
		else
		{
			Context->CurrentIO->ForwardPointsIndexed(Context);
			Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->In->GetOctree())); // Not sure this really saves perf
			Context->SetOperation(ReadyForNextParams);
		}
	}

	auto ProcessPoint = [&Context](int32 ReadIndex)
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
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

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

	if (Context->IsCurrentOperation(ReadyForNextParams))
	{
		if (Context->CurrentParams && Settings->bDebug) { DrawRelationsDebug(Context); }

		if (!Context->AdvanceParams())
		{
			Context->SetOperation(ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->CurrentParams->PrepareForPointData(Context->CurrentIO->Out);
			bProcessingAllowed = true;
		}
	}

	auto Initialize = [&Context]()
	{
		Context->SetOperation(ProcessingParams);
	};

	if (Context->IsCurrentOperation(ProcessingParams) || bProcessingAllowed)
	{
		bool bProcessingDone = PCGEx::Common::ParallelForLoop(Context, Context->CurrentIO->NumPoints, Initialize, ProcessPoint);

		if (bProcessingDone)
		{
			Context->SetOperation(ReadyForNextParams);

			if (Context->CurrentParams->bMarkMutualRelations)
			{
				//TODO, requires additional states & steps.
			}
		}
	}

	if (Context->IsCurrentOperation(Done))
	{
		Context->Points.OutputTo(Context);
		return true;
	}

	/*
	auto OnDataCopyBegin = [](FPCGExPointDataIO& PointIO, const int32 PointCount, const int32 IOIndex)	{		return true;	};
	auto OnPointCopied = [](FPCGPoint& OutPoint, FPCGExPointDataIO& PointIO, const int32 PointIndex)	{	};
	auto OnDataCopyEnd = [](FPCGExPointDataIO& PointIO, const int32 IOIndex)	{	};
	TArray<FPCGExPointDataIO> Pairs;
	FPCGExCommon::ForwardCopySourcePoints(Context, SourcePoints, Pairs, OnDataCopyBegin, OnPointCopied, OnDataCopyEnd);
	*/

	return false;
}

void FPCGExFindRelationsElement::DrawRelationsDebug(FPCGExFindRelationsContext* Context) const
{
#if WITH_EDITOR
	if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
	{
		Context->CurrentParams->PrepareForPointData(Context->CurrentIO->Out);
		PCGEx::Common::AsyncForLoop(Context, Context->CurrentIO->NumPoints, [&Context, &EditorWorld](int32 ReadIndex)
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
		});
	}
#endif
}

#undef LOCTEXT_NAMESPACE
