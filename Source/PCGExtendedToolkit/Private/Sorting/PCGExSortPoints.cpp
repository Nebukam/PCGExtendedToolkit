// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPoints.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGPoint.h"
#include "PCGContext.h"

#define LOCTEXT_NAMESPACE "PCGExSortPointsElement"

namespace PCGExSortPoints
{
	const FName SourceLabel = TEXT("Source");
}

#if WITH_EDITOR
FText UPCGExSortPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGExSortPointsTooltip", "Sort the source points according to specific rules.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExSortPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExSortPoints::SourceLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip",
	                                    "The order of the point in data will be changed, allowing to effectively rely on indices to perform index-bound operations, such as spline generation.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSortPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel,
	                                                                    EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip",
	                                    "The source points will be sorted according to specified options.");
#endif // WITH_EDITOR

	return PinProperties;
}

FPCGElementPtr UPCGExSortPointsSettings::CreateElement() const
{
	return MakeShared<FPCGExSortPointsElement>();
}

bool FPCGExSortPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsElement::Execute);

	const UPCGExSortPointsSettings* Settings = Context->GetInputSettings<UPCGExSortPointsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExSortPoints::SourceLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// first find the total Input bounds which will determine the size of each cell
	for (const FPCGTaggedData& Source : Sources)
	{
		// add the point bounds to the input cell

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

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();
		FPCGAsync::AsyncPointProcessing(Context, SourcePointData->GetPoints(), OutPoints,[](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			return true;
		});


		PCGExPointDataSorting::Sort(OutPoints, Settings->SortOver, Settings->SortDirection, Settings->SortOrder);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
