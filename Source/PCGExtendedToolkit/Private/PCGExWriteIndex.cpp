// Fill out your copyright notice in the Description page of Project Settings.

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "PCGExWriteIndex.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "PCGPin.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"

namespace PCGExWriteIndex
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGExWriteIndexSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExWriteIndexTooltip", "Write the current point index to an attribute.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExWriteIndex::SourceLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGExSourcePinTooltip",
	                                    "For each of the source points, their index position in the data will be written to an attribute.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::OutputPinProperties() const
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

FPCGElementPtr UPCGExWriteIndexSettings::CreateElement() const
{
	return MakeShared<FPCGExWriteIndexElement>();
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	const UPCGExWriteIndexSettings* Settings = Context->GetInputSettings<UPCGExWriteIndexSettings>();
	check(Settings);

	const FName AttributeName = Settings->AttributeName;

	if (AttributeName.IsNone() || AttributeName.ToString().IsEmpty() )
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NameEmpty", "Name cannot be \"None\" nor empty."));
		return true; // Skip execution
	}

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExWriteIndex::SourceLabel);
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
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;
		}

		// Initialize output dataset
		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		FPCGMetadataAttribute<int64>* IndexAttribute = OutputData->Metadata->FindOrCreateAttribute<int64>(
			AttributeName, -1, false);

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();

		int64 Index = 0;
		
		auto CopyAndAssignIndex = [OutputData, IndexAttribute, &Index](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutputData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
			IndexAttribute->SetValue(OutPoint.MetadataEntry, Index++);
			return true;
		};
		
		FPCGAsync::AsyncPointProcessing(Context, SourcePointData->GetPoints(), OutPoints, CopyAndAssignIndex);

		
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
