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

bool FPCGExFindRelationsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFindRelationsElement::Execute);

	const UPCGExFindRelationsSettings* Settings = Context->GetInputSettings<UPCGExFindRelationsSettings>();
	check(Settings);

	PCGExRelational::FParamsInputs ParamsInputs = PCGExRelational::FParamsInputs(Context, PCGExRelational::SourceRelationalParamsLabel);

	if (ParamsInputs.Params.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MissingParams", "Missing RelationsParams."));
		return true;
	}

	FPCGExPointIOMap<FPCGExIndexedPointDataIO> IOMap = FPCGExPointIOMap<FPCGExIndexedPointDataIO>(Context, PCGExRelational::SourceLabel, true);

	IOMap.ForEach(Context, [&Context, &Settings, &ParamsInputs](FPCGExIndexedPointDataIO* IO, const int32)
	{
		IO->ForwardPointsIndexed(Context);

		PCGExRelational::FPointProcessingData Data = PCGExRelational::FPointProcessingData{};
		Data.IO = IO;
		Data.Octree = const_cast<UPCGPointData::PointOctree*>(&IO->In->GetOctree());

		ParamsInputs.ForEach(Context, [&Context, &Settings, &Data](UPCGExRelationsParamsData* Params, const int32)
		{
			Data.Params = Params;
			Params->PrepareForPointData(Data.IO->Out);

			auto ProcessPoint = [&Data](int32 ReadIndex, int32 WriteIndex)
			{

				FPCGPoint InPoint = Data.IO->In->GetPoint(ReadIndex),
				          OutPoint = Data.IO->Out->GetPoint(ReadIndex);

				TArray<PCGExRelational::FSocketCandidate> Candidates;
				const double MaxDistance = PCGExRelational::Helpers::PrepareCandidatesForPoint(InPoint, Data, Candidates);
				
				auto ProcessPointNeighbor = [&ReadIndex, &Data, &Candidates](const FPCGPointRef& OtherPointRef)
				{
					const FPCGPoint* OtherPoint = OtherPointRef.Point;
					const int32 Index = Data.IO->GetIndex(OtherPoint->MetadataEntry);

					if (Index == ReadIndex) { return; }

					for (PCGExRelational::FSocketCandidate& SocketCandidate : Candidates)
					{
						if (SocketCandidate.ProcessPoint(OtherPoint)) { SocketCandidate.Index = Index; }
					}
				};

				const FBoxCenterAndExtent Box = FBoxCenterAndExtent(OutPoint.Transform.GetLocation(), FVector(MaxDistance));
				Data.Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

				Data.OutputTo(InPoint, Candidates);
				
			};

			const int32 NumIterations = Data.IO->In->GetPoints().Num();
			FPCGExCommon::AsyncForLoop(Context, NumIterations, ProcessPoint);

			if (Params->bMarkMutualRelations)
			{
				//TODO
			}

#if WITH_EDITOR
			if (Settings->bDebug)
			{
				if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
				{
					FPCGExCommon::ParallelForLoop(Context, NumIterations, [&Data, &EditorWorld](int32 ReadIndex, int32)
					{
						FPCGPoint PtA = Data.IO->In->GetPoint(ReadIndex);
						for (const PCGExRelational::FSocket& Socket : Data.Params->GetSocketMapping()->Sockets)
						{
							PCGExRelational::FSocketData SocketData = Socket.GetSocketData(PtA.MetadataEntry);
							if (SocketData.Index == -1) { continue; }

							FPCGPoint PtB = Data.IO->In->GetPoint(SocketData.Index);
							FVector Start = PtA.Transform.GetLocation();
							FVector End = FMath::Lerp(Start, PtB.Transform.GetLocation(), 0.4);
							DrawDebugLine(EditorWorld, Start, End, Socket.Descriptor->DebugColor, false, 10.0f, 0, 2);
						}
					});
				}
			}
#endif
		});

		IO->Indices.Empty();
	});

	IOMap.OutputTo(Context);

	/*
	auto OnDataCopyBegin = [](FPCGExPointDataIO& PointIO, const int32 PointCount, const int32 IOIndex)	{		return true;	};
	auto OnPointCopied = [](FPCGPoint& OutPoint, FPCGExPointDataIO& PointIO, const int32 PointIndex)	{	};
	auto OnDataCopyEnd = [](FPCGExPointDataIO& PointIO, const int32 IOIndex)	{	};
	TArray<FPCGExPointDataIO> Pairs;
	FPCGExCommon::ForwardCopySourcePoints(Context, SourcePoints, Pairs, OnDataCopyBegin, OnPointCopied, OnDataCopyEnd);
	*/

	return true;
}

#undef LOCTEXT_NAMESPACE
