// Fill out your copyright notice in the Description page of Project Settings.

#include "PCGExSortPoints.h"
#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "PCGExPointDataSorting.h"

UPCGExSortPoints::UPCGExSortPoints() {

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
void UPCGExSortPoints::ExecuteWithContext_Implementation(UPARAM(ref) FPCGContext& InContext, const FPCGDataCollection& Input, FPCGDataCollection& Output) {

	TArray<FPCGTaggedData> InputTaggedData = Input.GetInputsByPin(InPinPoints.Label);

	for (int i = 0; i < InputTaggedData.Num(); i++) {
		UPCGPointData* PointData = Cast<UPCGPointData>(InputTaggedData[i].Data);
		if (PointData) {
			//TArray<FPCGPoint>& PointList = const_cast<TArray<FPCGPoint>&>(PointData->ToPointDataWithContext(InContext)->GetPoints());
			TArray<FPCGPoint>& PointList = const_cast<TArray<FPCGPoint>&>(PointData->GetPoints()); //const_cast to avoid copying the array
			// Sort point list
			switch (SortOver)
			{
			case ESortDataSource::SOURCE_DENSITY:
				if (SortDirection == ESortDirection::ASCENDING) PointList.Sort(SortByDensity_ASC());
				else PointList.Sort(SortByDensity_DSC());
				break;
			case ESortDataSource::SOURCE_STEEPNESS:
				if (SortDirection == ESortDirection::ASCENDING) PointList.Sort(SortBySteepness_ASC());
				else PointList.Sort(SortBySteepness_DSC());
				break;
			case ESortDataSource::SOURCE_POSITION:
				if (SortDirection == ESortDirection::ASCENDING) {
					switch (SortOrder)
					{
					case ESortAxisOrder::AXIS_X_Y_Z: PointList.Sort(SortByPosition_XYZ_ASC()); break;
					case ESortAxisOrder::AXIS_Y_X_Z: PointList.Sort(SortByPosition_XYZ_ASC()); break;
					case ESortAxisOrder::AXIS_Z_X_Y: PointList.Sort(SortByPosition_XYZ_ASC()); break;
					case ESortAxisOrder::AXIS_X_Z_Y: PointList.Sort(SortByPosition_XYZ_ASC()); break;
					case ESortAxisOrder::AXIS_Y_Z_X: PointList.Sort(SortByPosition_XYZ_ASC()); break;
					case ESortAxisOrder::AXIS_Z_Y_X: PointList.Sort(SortByPosition_XYZ_ASC()); break;
					case ESortAxisOrder::AXIS_LENGTH:PointList.Sort(SortByPositionLength_ASC());break;
					default:break;
					}
				}
				else {
					switch (SortOrder)
					{
					case ESortAxisOrder::AXIS_X_Y_Z: PointList.Sort(SortByPosition_XYZ_DSC()); break;
					case ESortAxisOrder::AXIS_Y_X_Z: PointList.Sort(SortByPosition_XYZ_DSC()); break;
					case ESortAxisOrder::AXIS_Z_X_Y: PointList.Sort(SortByPosition_XYZ_DSC()); break;
					case ESortAxisOrder::AXIS_X_Z_Y: PointList.Sort(SortByPosition_XYZ_DSC()); break;
					case ESortAxisOrder::AXIS_Y_Z_X: PointList.Sort(SortByPosition_XYZ_DSC()); break;
					case ESortAxisOrder::AXIS_Z_Y_X: PointList.Sort(SortByPosition_XYZ_DSC()); break;
					case ESortAxisOrder::AXIS_LENGTH:PointList.Sort(SortByPositionLength_DSC());break;
					default:break;
					}
				}
				break;
			case ESortDataSource::SOURCE_SCALE:
				break;
			default:
				break;
			}

			//PointData->SetPoints(PointList);
		}
	}

	Output = Input;

}