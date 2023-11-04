// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational/PCGExCaptureNeighbors.h"

#include "Relational/PCGExRelationalData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "PCGExCommon.h"
#include "UnrealEd.h"

#define LOCTEXT_NAMESPACE "PCGExCaptureNeighbors"

#if WITH_EDITOR
FText UPCGExCaptureNeighborsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExCaptureNeighborsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	return PinProperties;
}

FPCGElementPtr UPCGExCaptureNeighborsSettings::CreateElement() const
{
	return MakeShared<FPCGExCaptureNeighborsElement>();
}

bool FPCGExCaptureNeighborsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCaptureNeighborsElement::Execute);

	const UPCGExRelationalData* RelationalData = GetFirstRelationalData(Context);
	if (!RelationalData) { return true; }

	const UPCGExCaptureNeighborsSettings* Settings = Context->GetInputSettings<UPCGExCaptureNeighborsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExRelational::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<FPCGExSelectorSettingsBase> Modifiers;
	TArray<FPCGExSamplingCandidateData> Candidates;
	TMap<int64, int32> Indices;

	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(Source.Data);

		if (!InSpatialData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* InPointData = InSpatialData->ToPointData(Context);
		if (!InPointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		if (InPointData->Metadata->HasAttribute(RelationalData->RelationalIdentifier))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("RelationalAttributeAlreadyExists", "Relational attribute '{0}' already exists in source '{1}, it will be overwritten.'"), FText::FromName(RelationalData->RelationalIdentifier),FText::FromString(Source.Data->GetClass()->GetName())));
		}

		// Initialize output dataset
		UPCGPointData* OutPointData = NewObject<UPCGPointData>();
		OutPointData->InitializeFromData(InPointData);
		Outputs.Add_GetRef(Source).Data = OutPointData;
		const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();

		FPCGMetadataAttribute<FPCGExRelationAttributeData>* RelationalAttribute = PrepareData<FPCGExRelationAttributeData>(RelationalData, OutPointData);
		const UPCGPointData::PointOctree& Octree = InPointData->GetOctree();

		int32 CurrentIndex = 0;
		FPCGPoint* CurrentPoint = nullptr;

		Candidates.Reserve(RelationalData->Slots.Num());
		bool bUseModifiers = RelationalData->PrepareModifiers(InPointData, Modifiers);

		Indices.Empty(InPoints.Num());
		for (int i = 0; i < InPoints.Num(); i++) { Indices.Add(InPoints[i].MetadataEntry, i); }

		double LocalMaxDistance = -1;

		auto ProcessSingleNeighbor = [&CurrentPoint, &Candidates, &LocalMaxDistance, &Indices](
			const FPCGPointRef& OtherPointRef)
		{
			// Skip "self"
			const FPCGPoint* OtherPoint = OtherPointRef.Point;

			if (CurrentPoint == OtherPointRef.Point) { return; }

			int64 Key = OtherPoint->MetadataEntry;
			// Loop over slots inside the Octree sampling
			// Still expensive, but greatly reduce the number of iteration vs sampling the octree each slot.
			for (FPCGExSamplingCandidateData& CandidateData : Candidates)
			{
				if (CandidateData.ProcessPoint(OtherPoint))
				{
					//TODO: Is there a more memory-efficient way to retrieve the index?
					CandidateData.Index = *Indices.Find(Key);
				}
			}
		};

		TArray<FPCGPoint>& OutPoints = OutPointData->GetMutablePoints();
		FPCGAsync::AsyncPointProcessing(Context, InPoints, OutPoints, [&Candidates, &Modifiers, &CurrentIndex, &CurrentPoint, &RelationalData, &LocalMaxDistance, &Octree, &OutPointData, &ProcessSingleNeighbor, RelationalAttribute, &bUseModifiers](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			double GreatestMaxDistance = RelationalData->GreatestStaticMaxDistance;

			OutPoint = InPoint;
			CurrentPoint = &OutPoint;

			OutPointData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			const double MaxDistance = RelationalData->InitializeCandidatesForPoint(Candidates, OutPoint, bUseModifiers, Modifiers);

			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(OutPoint.Transform.GetLocation(), FVector(MaxDistance));
			Octree.FindElementsWithBoundsTest(Box, ProcessSingleNeighbor);

			const FPCGExRelationAttributeData Data = FPCGExRelationAttributeData(CurrentIndex, &Candidates);
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
			// In a function or method where you want to get the viewport world:
			UWorld* W = GEditor->GetEditorWorldContext().World();

			if (W)
			{
				int ValueKey = 0;
				for (const FPCGPoint& Point : OutPoints)
				{
					FPCGExRelationAttributeData Data = RelationalAttribute->GetValue(ValueKey); //Point.MetadataEntry is offset at this point, which is annoying and weird.
					for (int i = 0; i < Data.Indices.Num(); i++)
					{
						const int64 NeighborIndex = Data.Indices[i];
						if (NeighborIndex == -1) { continue; }
						FVector Start = Point.Transform.GetLocation();
						FVector End = FMath::Lerp(Start, OutPoints[NeighborIndex].Transform.GetLocation(), 0.4);
						DrawDebugLine(W, Start, End, RelationalData->Slots[i].DebugColor, false, 10.0f, 0, 2);
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
