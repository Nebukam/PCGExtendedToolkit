// Fill out your copyright notice in the Description page of Project Settings.

#include "Pathfinding/PCGExPathfinding.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"

UPCGExPathfinding::UPCGExPathfinding()
{
	bExposeToLibrary = true;
	bHasDefaultInPin = false;
	bHasDefaultOutPin = false;

	// NODE INPUTS
	// Source points
	InPinPoints = FPCGPinProperties(NAME_SOURCE_POINTS, EPCGDataType::Point);
	CustomInputPins.Add(InPinPoints);

	// Start point
	InPinStartPoint = FPCGPinProperties(NAME_START_POINT, EPCGDataType::Point, false, false);
	CustomInputPins.Add(InPinStartPoint);

	// End point
	InPinEndPoint = FPCGPinProperties(NAME_END_POINT, EPCGDataType::Point, false, false);
	CustomInputPins.Add(InPinEndPoint);

	//NODE OUTPUTS
	//Out points
	OutPinPoints = FPCGPinProperties(NAME_OUT_POINTS, EPCGDataType::Point);
	CustomOutputPins.Add(OutPinPoints);
}

/**
 * ~End UObject interface
 *
 * NOTE: This function is linked to BlueprintNativeEvent: UPCGBlueprintElement::ExecuteWithContext
 */
void UPCGExPathfinding::ExecuteWithContext_Implementation(
	UPARAM(ref) FPCGContext& InContext,
	const FPCGDataCollection& Input,
	FPCGDataCollection& Output)
{
	Output = Input;
}

/**
 * Please add a function description
 *
 * NOTE: This function is linked to BlueprintImplementableEvent: UPCGBlueprintElement::PointLoopBody
 */
bool UPCGExPathfinding::PointLoopBody_Implementation(
	const FPCGContext& InContext,
	const UPCGPointData* InData,
	const FPCGPoint& InPoint,
	FPCGPoint& OutPoint,
	UPCGMetadata* OutMetadata) const
{
	OutPoint = InPoint;
	return true;
}
