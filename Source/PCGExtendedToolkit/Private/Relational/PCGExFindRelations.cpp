// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExFindRelations.h"

#include "Data/PCGExRelationalData.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Data/PCGExRelationalDataHelpers.h"

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

	TArray<FPCGTaggedData> SourcePoints = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);
	TArray<FPCGTaggedData> SourceParams = Context->InputData.GetInputsByPin(PCGExRelational::SourceRelationalParamsLabel);
	
	FPCGExPointIOMap<FPCGExIndexedPointDataIO> IOMap = FPCGExPointIOMap<FPCGExIndexedPointDataIO>(Context, SourcePoints, true);
		
	IOMap.ForEachPair(Context, [&Context](FPCGExIndexedPointDataIO* PointDataIO, const int32)
	{
		PointDataIO->ForwardPointsIndexed(Context, [](FPCGPoint& Point, const int32 Index){});

		//ProcessParams(); -> Need to process for each param

		//Then flush indices
		// but we don't want to maintain a shit load of indice map in memory, we can afford to process them one after another
	});

	FPCGExRelationalIOMap RelationalIOMap = FPCGExRelationalIOMap(Context, IOMap); //This is specific to a given param
	
	auto OnDataCopyBegin = [](FPCGExPointDataIO& PointIO, const int32 PointCount, const int32 IOIndex)
	{
		return true;
	};

	auto OnPointCopied = [](FPCGPoint& OutPoint, FPCGExPointDataIO& PointIO, const int32 PointIndex)
	{
		
	};

	auto OnDataCopyEnd = [](FPCGExPointDataIO& PointIO, const int32 IOIndex)
	{

	};

	TArray<FPCGExPointDataIO> Pairs;
	FPCGExCommon::ForwardSourcePoints(Context, SourcePoints, Pairs, OnDataCopyBegin, OnPointCopied, OnDataCopyEnd);

	IOMap.OutputTo();
	
	return true;
}

void ProcessParams(FPCGContext* Context, UPCGExRelationalParamsData* Params)
{
const UPCGExFindRelationsSettings* Settings = Context->GetInputSettings<UPCGExFindRelationsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);

	FPCGExRelationalPair CurrentRelationalPair = FPCGExRelationalPair{};
	FPCGExProcessingData Data = FPCGExProcessingData{};
	Data.RelationalPair = &CurrentRelationalPair;
	Data.Params = Params;

	auto OnDataCopyBegin = [Context, &Data](FPCGExPointDataIO& PointIO, const int32 PointCount, const int32 IOIndex)
	{
		Data.RelationalPair->Capture(PointIO);
		FPCGExRelationalDataHelpers::CreateRelationalDataOutput(Context, Data.Params, *Data.RelationalPair);

		const UPCGPointData::PointOctree& Octree = PointIO.In->GetOctree();
		Data.Octree = const_cast<UPCGPointData::PointOctree*>(&Octree);
		Data.GetIndices().Empty(PointCount);

		Data.RelationalPair->RelationalData->GetRelations().Reserve(PointCount);
		Data.bUseModifiers = Data.Params->PrepareSelectors(PointIO.In, Data.GetModifiers());

		return true;
	};

	FPCGPoint CurrentPoint;

	auto ProcessPointNeighbor = [&CurrentPoint, &Data](const FPCGPointRef& OtherPointRef)
	{
		const FPCGPoint* OtherPoint = OtherPointRef.Point;
		const int32 Index = Data.GetIndex(OtherPoint->MetadataEntry);

		if (Index == Data.CurrentIndex) { return; } // Skip self

		for (FPCGExRelationCandidate& CandidateData : Data.GetCandidates())
		{
			if (CandidateData.ProcessPoint(OtherPoint)) { CandidateData.Index = Index; }
		}
	};

	auto OnPointCopied = [&Data](FPCGPoint& OutPoint, FPCGExPointDataIO& PointIO, const int32 PointIndex)
	{
		Data.GetIndices().Add(OutPoint.MetadataEntry, PointIndex);
	};

	auto FindAndProcessNeighbors = [&Data, &ProcessPointNeighbor](int32 ReadIndex, int32 WriteIndex)
	{
		Data.CurrentIndex = ReadIndex;

		FPCGPoint InPoint = Data.RelationalPair->InPoints->GetPoint(ReadIndex),
		          OutPoint = Data.RelationalPair->OutPoints->GetPoint(ReadIndex);

		const double MaxDistance = FPCGExRelationalDataHelpers::PrepareCandidatesForPoint(InPoint, Data);

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(OutPoint.Transform.GetLocation(), FVector(MaxDistance));
		Data.Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		Data.RelationalPair->RelationalData->GetRelations().Emplace_GetRef(ReadIndex, &Data.GetCandidates());
	};

	auto OnDataCopyEnd = [&Context, &Data, &FindAndProcessNeighbors, &Settings](FPCGExPointDataIO& PointIO, const int32 IOIndex)
	{
		FPCGExCommon::AsyncForLoop(Context, PointIO.In->GetPoints().Num(), FindAndProcessNeighbors);

		if (Data.Params->bMarkMutualRelations)
		{
			//TODO
		}

#if WITH_EDITOR
		if (Settings->bDebug)
		{
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				FPCGExCommon::AsyncForLoop(Context, PointIO.In->GetPoints().Num(), [&Data, &EditorWorld](int32 ReadIndex, int32)
				{
					FPCGPoint PtA = Data.RelationalPair->InPoints->GetPoint(ReadIndex);
					const FPCGExRelationData& PointData = *Data.RelationalPair->RelationalData->GetRelation(ReadIndex);
					int32 SlotIndex = -1;
					for (const FPCGExRelationDetails& Details : PointData.Details)
					{
						SlotIndex++;
						if (Details.Index == -1) { continue; }

						FPCGPoint PtB = Data.RelationalPair->InPoints->GetPoint(Details.Index);
						FVector Start = PtA.Transform.GetLocation();
						FVector End = FMath::Lerp(Start, PtB.Transform.GetLocation(), 0.4);
						DrawDebugLine(EditorWorld, Start, End, Data.Params->RelationSlots[SlotIndex].DebugColor, false, 10.0f, 0, 2);
					}
				});
			}
		}
#endif
	};

	TArray<FPCGExPointDataIO> Pairs;
	FPCGExCommon::ForwardSourcePoints(Context, Sources, Pairs, OnDataCopyBegin, OnPointCopied, OnDataCopyEnd);
	
}

#undef LOCTEXT_NAMESPACE
