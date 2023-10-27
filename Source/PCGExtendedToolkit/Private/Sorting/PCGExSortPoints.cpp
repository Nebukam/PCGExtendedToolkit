// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting/PCGExSortPoints.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGPoint.h"

#define ATTRIBUTE_CHECK(_ATT) \
if (Settings->SortDirection == ESortDirection::Ascending) OutPoints.Sort(SortBy##_ATT##_Asc()); else OutPoints.Sort(SortBy##_ATT##_Dsc());

#define SWITCH_ORDER_CASE(_ATT, _ENUM, _A, _B, _C, _ORDER) \
case ESortAxisOrder::Axis_##_ENUM##: OutPoints.Sort(SortBy##_ATT##_##_A##_B##_C##_##_ORDER()); break;\

#define SWITCH_ORDER_3(_ATT, _A, _B, _C, _SUFFIX) \
SWITCH_ORDER_CASE(_TYPE, X_Y_Z, _A, _B, _C, _ORDER) \
SWITCH_ORDER_CASE(_TYPE, X_Z_Y, _A, _C, _B, _ORDER) \
SWITCH_ORDER_CASE(_TYPE, Y_X_Z, _B, _A, _C, _ORDER) \
SWITCH_ORDER_CASE(_TYPE, Y_Z_X, _B, _C, _A, _ORDER) \
SWITCH_ORDER_CASE(_TYPE, Z_X_Y, _C, _A, _B, _ORDER) \
SWITCH_ORDER_CASE(_TYPE, Z_Y_X, _C, _B, _A, _ORDER)

#define AXIS_CHECK_BASE(_ATT, _ORDER) \
switch (Settings->SortOrder){\
SWITCH_ORDER_3(_ATT, X, Y, Z, _ORDER) \
case ESortAxisOrder::Axis_Length: OutPoints.Sort(SortBy##_ATT##Length_##_ORDER()); break;\
default: break;}
#define AXIS_CHECK(_ATT) \
if (Settings->SortDirection == ESortDirection::Ascending) { AXIS_CHECK_BASE(_ATT, Asc) } else { AXIS_CHECK_BASE(_ATT, Dsc) }
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

		FPCGPoint pt;
		pt.Transform.GetLocation()
		
		// Copy input to output
		FPCGAsync::AsyncPointProcessing(
			Context, SourcePointData->GetPoints(), OutPoints,
			[OutputData](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint)
			{
				OutPoint = SourcePoint;
				return true;
			}
		);

		switch (Settings->SortOver) {
		case EPCGPointProperties::Density:
			ATTRIBUTE_CHECK(Density)
			break;
		case EPCGPointProperties::BoundsMin:
			break;
		case EPCGPointProperties::BoundsMax:
			break;
		case EPCGPointProperties::Extents:
			break;
		case EPCGPointProperties::Color:
			break;
		case EPCGPointProperties::Position:
			AXIS_CHECK(Position)
			break;
		case EPCGPointProperties::Rotation:
			break;
		case EPCGPointProperties::Scale:
			AXIS_CHECK(Scale)
			break;
		case EPCGPointProperties::Transform:
			AXIS_CHECK(Position)
			break;
		case EPCGPointProperties::Steepness:
			ATTRIBUTE_CHECK(Steepness)
			break;
		case EPCGPointProperties::LocalCenter:
			break;
		case EPCGPointProperties::Seed:
			ATTRIBUTE_CHECK(Seed)
			break;
		default: ;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
