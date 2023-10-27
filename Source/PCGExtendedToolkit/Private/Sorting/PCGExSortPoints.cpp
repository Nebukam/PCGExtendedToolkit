// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPoints.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

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
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExSortPoints::SourceLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip", "The order of the point in data will be changed, allowing to effectively rely on indices to perform index-bound operations, such as spline generation.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSortPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "The source points will be sorted according to specified options.");
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
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;			
		}

		// Initialize output dataset
		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		// Copy input to output
		FPCGAsync::AsyncPointProcessing(Context, SourcePointData->GetPoints(), OutputData->GetMutablePoints(),
			[OutputData](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint) {
				OutPoint = SourcePoint;
				return true;
			}
		);

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();

		switch (Settings->SortOver)
		{
		case ESortDataSource::SOURCE_DENSITY:
			if (Settings->SortDirection == ESortDirection::ASCENDING) OutPoints.Sort(SortByDensity_ASC());
			else OutPoints.Sort(SortByDensity_DSC());
			break;
		case ESortDataSource::SOURCE_STEEPNESS:
			if (Settings->SortDirection == ESortDirection::ASCENDING) OutPoints.Sort(SortBySteepness_ASC());
			else OutPoints.Sort(SortBySteepness_DSC());
			break;
		case ESortDataSource::SOURCE_POSITION:
			if (Settings->SortDirection == ESortDirection::ASCENDING) {
				switch (Settings->SortOrder)
				{
				case ESortAxisOrder::AXIS_X_Y_Z: OutPoints.Sort(SortByPosition_XYZ_ASC()); break;
				case ESortAxisOrder::AXIS_Y_X_Z: OutPoints.Sort(SortByPosition_XYZ_ASC()); break;
				case ESortAxisOrder::AXIS_Z_X_Y: OutPoints.Sort(SortByPosition_XYZ_ASC()); break;
				case ESortAxisOrder::AXIS_X_Z_Y: OutPoints.Sort(SortByPosition_XYZ_ASC()); break;
				case ESortAxisOrder::AXIS_Y_Z_X: OutPoints.Sort(SortByPosition_XYZ_ASC()); break;
				case ESortAxisOrder::AXIS_Z_Y_X: OutPoints.Sort(SortByPosition_XYZ_ASC()); break;
				case ESortAxisOrder::AXIS_LENGTH:OutPoints.Sort(SortByPositionLength_ASC());break;
				default:break;
				}
			}
			else {
				switch (Settings->SortOrder)
				{
				case ESortAxisOrder::AXIS_X_Y_Z: OutPoints.Sort(SortByPosition_XYZ_DSC()); break;
				case ESortAxisOrder::AXIS_Y_X_Z: OutPoints.Sort(SortByPosition_XYZ_DSC()); break;
				case ESortAxisOrder::AXIS_Z_X_Y: OutPoints.Sort(SortByPosition_XYZ_DSC()); break;
				case ESortAxisOrder::AXIS_X_Z_Y: OutPoints.Sort(SortByPosition_XYZ_DSC()); break;
				case ESortAxisOrder::AXIS_Y_Z_X: OutPoints.Sort(SortByPosition_XYZ_DSC()); break;
				case ESortAxisOrder::AXIS_Z_Y_X: OutPoints.Sort(SortByPosition_XYZ_DSC()); break;
				case ESortAxisOrder::AXIS_LENGTH:OutPoints.Sort(SortByPositionLength_DSC());break;
				default:break;
				}
			}
			break;
		case ESortDataSource::SOURCE_SCALE:
			break;
		default:
			break;
		}
		
		
	}

	return true;
}

#undef LOCTEXT_NAMESPACE