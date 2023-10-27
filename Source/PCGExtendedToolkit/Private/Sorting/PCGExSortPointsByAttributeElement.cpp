// Fill out your copyright notice in the Description page of Project Settings.


#include "Sorting\PCGExSortPointsByAttributeElement.h"

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAccessor.h"

UPCGExSortPointsByAttributeElement::UPCGExSortPointsByAttributeElement() {

	bExposeToLibrary = true;
	bHasDefaultInPin = false;
	bHasDefaultOutPin = false;

	// NODE INPUTS
	// Source points
	InPinPoints = FPCGPinProperties(NAME_SOURCE_POINTS, EPCGDataType::Point);
	CustomInputPins.Add(InPinPoints);

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
void UPCGExSortPointsByAttributeElement::ExecuteWithContext_Implementation(UPARAM(ref) FPCGContext& InContext, const FPCGDataCollection& Input, FPCGDataCollection& Output) {

	TArray<FPCGTaggedData> InputTaggedData = Input.GetInputsByPin(InPinPoints.Label);

	for (int i = 0; i < InputTaggedData.Num(); i++) {
		UPCGPointData* PointData = Cast<UPCGPointData>(InputTaggedData[i].Data);
		if (PointData) {
			UPCGMetadata* Metadata = PointData->Metadata;
			//UPCGMetadataAccessorHelpers::GetFloatAttribute()
			//Metadata->
			TArray<FPCGPoint>& PointList = const_cast<TArray<FPCGPoint>&>(PointData->GetPoints());			
		}
	}

	Output = Input;
}

bool UPCGExSortPointsByAttributeElement::PointLoopBody_Implementation(const FPCGContext& InContext,
	const UPCGPointData* InData, const FPCGPoint& InPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	return true;
}
