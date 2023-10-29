// Fill out your copyright notice in the Description page of Project Settings.

#include "Relational\PCGCreateDirectionalRelationships.h"
#include "Relational\DataTypes.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGPin.h"
#include "PCGExCommon.h"

#define LOCTEXT_NAMESPACE "PCGDirectionalRelationships"

namespace PCGDirectionalRelationships
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGDirectionalRelationships::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDirectionalRelationshipsTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGDirectionalRelationships::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGDirectionalRelationships::SourceLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip",
	                                    "For each of the source points, their index position in the data will be written to an attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGDirectionalRelationships::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGExOutputPinTooltip",
	                                    "The source points will be output with the newly added attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGElementPtr UPCGDirectionalRelationships::CreateElement() const
{
	return MakeShared<FPCGDirectionalRelationships>();
}

bool FPCGDirectionalRelationships::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDirectionalRelationships::Execute);

	const UPCGDirectionalRelationships* Settings = Context->GetInputSettings<UPCGDirectionalRelationships>();
	check(Settings);

	const FDirectionalRelationSlotListSettings SlotsSettings = Settings->Slots;
	const float ExtentLength = Settings->CheckExtent;
	const FName IndexAttributeName = Settings->IndexAttributeName;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGDirectionalRelationships::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* SourcePointData = SourceData->ToPointData(Context);
		if (!SourcePointData)
		{
			PCGE_LOG(Error, GraphAndLog,
			         LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		// Initialize output dataset
		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		bool bIndexAttributeExists = OutputData->Metadata->HasAttribute(IndexAttributeName);
		FPCGMetadataAttribute<int64>* IndexAttribute = OutputData->Metadata->FindOrCreateAttribute<int64>(
			IndexAttributeName, -1, false);

		TArray<const FPCGMetadataAttribute<FDirectionalRelationData>*> SlotAttributes =
			DataTypeHelpers::FindOrCreateAttributes(SlotsSettings, OutputData);

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();

		//TODO: Check if an "INDEX" attribute is provided

		if (bIndexAttributeExists)
		{
			PCGEX_COPY_POINTS(SourcePointData->GetPoints(), OutPoints, {
			                  OutputData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			                  }, OutputData)
		}
		else
		{
			PCGE_LOG(Warning, GraphAndLog,
			         LOCTEXT("InvalidIndexAttribute", "Could not find a valid index attribute, creating one on the fly."
			         ));
			int64 Index = 0;
			PCGEX_COPY_POINTS(SourcePointData->GetPoints(), OutPoints, {
			                  OutputData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			                  //TODO: Set index attribute
			                  IndexAttribute->SetValue(OutPoint.MetadataEntry, Index);
			                  Index++;
			                  }, OutputData, &Index, IndexAttribute)
		}


		// Get octree after copy
		const UPCGPointData::PointOctree& Octree = OutputData->GetOctree();
		const FVector BaseExtent = FVector{ExtentLength, ExtentLength, ExtentLength};

		// Init candidates data struct
		TArray<FSlotCandidateData> CandidatesSlots;
		CandidatesSlots.Reserve(SlotsSettings.Num());
		for (int i = 0; i < SlotsSettings.Num(); i++)
		{
			FDirectionalRelationSlotSettings SSett = SlotsSettings[i];
			FPCGMetadataAttribute<int64>* SlotAttribute = OutputData->Metadata->FindOrCreateAttribute<int64>(
			SSett.AttributeName, -1, false);
			
			FSlotCandidateData Candidates = FSlotCandidateData{SlotAttribute};
			
			CandidatesSlots.Add(Candidates);
		}

		TArray<FDirectionalRelationSlotSettings> Slots = SlotsSettings.Slots;
		FPCGPoint NullNode = FPCGPoint{};
		FPCGPoint& CurrentNode = NullNode;
		FPCGPoint& BestCandidate = NullNode;
		FVector Origin;

		auto ProcessNeighbor = [&Slots, &SlotsSettings, &CurrentNode, &Origin, &CandidatesSlots](
			const FPCGPointRef& TargetPointRef)
		{
			// If the source pointer and target pointer are the same, ignore distance to the exact same point
			if (&CurrentNode == TargetPointRef.Point) { return; }

			for (int i = 0; i < Slots.Num(); i++)
			{
				FDirectionalRelationSlotSettings CurrentSettings = Slots[i];
				FSlotCandidateData CurrentSlotData = CandidatesSlots[i];
				//TODO: For each slot settings, update & compare against the matching Candidate data in CandidatesSlots
			}
		};

		auto InnerLoop = [&Slots, OutputData, Octree, &Origin, BaseExtent, &NullNode, &CurrentNode, &CandidatesSlots,
				ProcessNeighbor](int32 Index, FPCGPoint& Point)
		{
			CurrentNode = Point;
			FBoxCenterAndExtent BCE = FBoxCenterAndExtent{Point.Transform.GetLocation(), BaseExtent};
			Octree.FindElementsWithBoundsTest(BCE, ProcessNeighbor);

			for (int i = 0; i < Slots.Num(); i++)
			{
				FDirectionalRelationSlotSettings CurrentSettings = Slots[i];
				FSlotCandidateData CurrentSlotData = CandidatesSlots[i];
				// TODO: "Apply" CandidatesSlots data into attributes.
			}
			
			return true;
		};

		FPCGAsync::AsyncPointProcessing(Context, static_cast<int32>(OutPoints.Num()), OutPoints, InnerLoop);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
