// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExFindRelations.h"

#include "Data/PCGExRelationalData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
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

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UPCGExRelationalParamsData* Params = GetRelationalParams(Context);
	if (!Params) { return true; }

	TArray<FPCGExPointDataPair> PointDatas;
	FPCGExRelationalPair CurrentRelationalPair = FPCGExRelationalPair{};
	bool bUseModifiers = false;

	PCGExFindRelations::FPCGExProcessingData Data = PCGExFindRelations::FPCGExProcessingData{};
	Data.Params = Params;
	
	auto OnDataWillBeginCopy = [Context, &Params, &CurrentRelationalPair, &bUseModifiers, &Data](const int32 Index, const int32 PointCount, FPCGExPointDataPair& Pair)
	{
		// Create matching relation data output here
		CurrentRelationalPair.Capture(Pair);
		FPCGExRelationalDataHelpers::CreateRelationalDataOutput(Context, Params, CurrentRelationalPair);

		const UPCGPointData::PointOctree& Octree = Pair.SourcePointData->GetOctree();
		Data.Octree = const_cast<UPCGPointData::PointOctree*>(&Octree);
		Data.RelationalData = CurrentRelationalPair.RelationalData;
		Data.GetIndices().Empty(PointCount);

		CurrentRelationalPair.RelationalData->GetRelations().Reserve(PointCount);
		bUseModifiers = Params->PrepareSelectors(Pair.SourcePointData, Data.GetModifiers());

		return true;
	};

	FPCGPoint CurrentPoint;
	
	auto ProcessSingleNeighbor = [&CurrentPoint, &Data](
			const FPCGPointRef& OtherPointRef)
	{
		// Skip "self"
		const FPCGPoint* OtherPoint = OtherPointRef.Point;

		if (CurrentPoint == OtherPointRef.Point) { return; }

		const int64 Key = OtherPoint->MetadataEntry;
		// Loop over slots inside the Octree sampling
		// Still expensive, but greatly reduce the number of iteration vs sampling the octree each slot.
		for (FPCGExRelationCandidate& CandidateData : Data.GetCandidates())
		{
			if (CandidateData.ProcessPoint(OtherPoint))
			{
				//TODO: Is there a more memory-efficient way to retrieve the index?
				CandidateData.Index = *(Data.GetIndices().Find(Key));
			}
		}
	};
	
	auto OnPointCopied = [&Params, &CurrentRelationalPair, &Candidates, &bUseModifiers, &Modifiers, &Data](const int32 Index, const FPCGPoint& InPoint, FPCGPoint& OutPoint, FPCGExPointDataPair& Pair)
	{
		CurrentRelationalPair.RelationalData->PrepareCandidatesForPoint(Candidates, InPoint, bUseModifiers, Modifiers);
		Data.Indices.Add(InPoint.MetadataEntry, Index);
		
		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(OutPoint.Transform.GetLocation(), FVector(MaxDistance));
		Data.Octree->FindElementsWithBoundsTest(Box, ProcessSingleNeighbor);

		FPCGExRelationData& Data = CurrentRelationalPair.RelationalData->GetRelations().Emplace_GetRef(Index, &Candidates);
	};

	auto OnPointsCopied = [](const int32 Index, FPCGExPointDataPair& Pair)
	{
	};

	TArray<FPCGExPointDataPair> Pairs;
	FPCGExCommon::ForwardSourcePoints(Context, Sources, Pairs, OnDataWillBeginCopy, OnPointCopied, OnPointsCopied);

////////////////
	
	TArray<FPCGExSamplingModifier> Modifiers;
	TArray<FPCGExRelationCandidate> Candidates;
	TMap<int64, int32> Indices;

	for (const FPCGTaggedData& Source : Sources)
	{
		// Initialize output dataset
		UPCGPointData* OutPointData = NewObject<UPCGPointData>();
		OutPointData->InitializeFromData(InPointData);
		Outputs.Add_GetRef(Source).Data = OutPointData;

		const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();

		FPCGMetadataAttribute<FPCGExRelationData>* RelationalAttribute = PrepareDataAttributes_UNSUPPORTED<FPCGExRelationData>(RelationalData, OutPointData);
		const UPCGPointData::PointOctree& Octree = InPointData->GetOctree();

		int32 CurrentIndex = 0;
		FPCGPoint* CurrentPoint = nullptr;

		Candidates.Reserve(RelationalData->RelationSlots.Num());
		bool bUseModifiers = RelationalData->PrepareSelectors(InPointData, Modifiers);

		Indices.Empty(InPoints.Num());
		for (int i = 0; i < InPoints.Num(); i++) { Indices.Add(InPoints[i].MetadataEntry, i); }

		auto ProcessSingleNeighbor = [&CurrentPoint, &Candidates, &Indices](
			const FPCGPointRef& OtherPointRef)
		{
			// Skip "self"
			const FPCGPoint* OtherPoint = OtherPointRef.Point;

			if (CurrentPoint == OtherPointRef.Point) { return; }

			const int64 Key = OtherPoint->MetadataEntry;
			// Loop over slots inside the Octree sampling
			// Still expensive, but greatly reduce the number of iteration vs sampling the octree each slot.
			for (FPCGExRelationCandidate& CandidateData : Candidates)
			{
				if (CandidateData.ProcessPoint(OtherPoint))
				{
					//TODO: Is there a more memory-efficient way to retrieve the index?
					CandidateData.Index = *Indices.Find(Key);
				}
			}
		};

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		FPCGAsync::AsyncPointProcessing(Context, InPoints, OutPoints, [&Candidates, &Modifiers, &CurrentIndex, &CurrentPoint, &RelationalData, &Octree, &OutPointData, &ProcessSingleNeighbor, RelationalAttribute, &bUseModifiers](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			CurrentPoint = &OutPoint;

			OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			const double MaxDistance = RelationalData->PrepareCandidatesForPoint(Candidates, OutPoint, bUseModifiers, Modifiers);

			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(OutPoint.Transform.GetLocation(), FVector(MaxDistance));
			Octree.FindElementsWithBoundsTest(Box, ProcessSingleNeighbor);

			const FPCGExRelationData Data = FPCGExRelationData(CurrentIndex, &Candidates);
			RelationalAttribute->SetValue(OutPoint.MetadataEntry, Data);

			CurrentIndex++;
			return true;
		});

		if (RelationalData->bMarkMutualRelations)
		{
			//TODO -- Will require to build an accessor to access the value as Ref
		}

#if WITH_EDITOR
		if (Settings->bDebug)
		{
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				int ValueKey = 0;
				for (const FPCGPoint& Point : OutPoints)
				{
					FPCGExRelationData Data = RelationalAttribute->GetValue(ValueKey); //Point.MetadataEntry is offset at this point, which is annoying and weird.
					for (int i = 0; i < Data.Details.Num(); i++)
					{
						const int64 NeighborIndex = Data.Details[i].Index;
						if (NeighborIndex == -1) { continue; }

						FVector Start = Point.Transform.GetLocation();
						FVector End = FMath::Lerp(Start, OutPoints[NeighborIndex].Transform.GetLocation(), 0.4);
						DrawDebugLine(EditorWorld, Start, End, RelationalData->RelationSlots[i].DebugColor, false, 10.0f, 0, 2);
					}
					ValueKey++;
				}
			}
		}
#endif
	}


	return true;
}

#undef LOCTEXT_NAMESPACE
