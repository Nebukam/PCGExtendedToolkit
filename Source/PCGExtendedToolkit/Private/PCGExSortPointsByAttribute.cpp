// Fill out your copyright notice in the Description page of Project Settings.


#include "PCGExSortPointsByAttribute.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"

UPCGExSortPointsByAttribute::UPCGExSortPointsByAttribute() {

	bHasDefaultInPin = false;
	bHasDefaultOutPin = false;

	// NODE INPUTS
	// Source points
	InputPinPoints = FPCGPinProperties(NAME_SOURCE_POINTS, EPCGDataType::Point);
	CustomInputPins.Add(InputPinPoints);

	//NODE OUTPUTS
	//Out points
	OutputPinPoints = FPCGPinProperties(NAME_OUT_POINTS, EPCGDataType::Point);
	CustomOutputPins.Add(OutputPinPoints);

}

/**
 * ~End UObject interface
 *
 * NOTE: This function is linked to BlueprintNativeEvent: UPCGBlueprintElement::ExecuteWithContext
 */
void UPCGExSortPointsByAttribute::ExecuteWithContext_Implementation(UPARAM(ref) FPCGContext& InContext, const FPCGDataCollection& Input, FPCGDataCollection& Output) {
	Output = Input;
}

/**
 * Please add a function description
 *
 * NOTE: This function is linked to BlueprintImplementableEvent: UPCGBlueprintElement::PointLoopBody
 */
bool UPCGExSortPointsByAttribute::PointLoopBody_Implementation(const FPCGContext& InContext, const UPCGPointData* InData, const FPCGPoint& InPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const {
	OutPoint = InPoint;
	return true;
}