// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PCGData.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "PCGExCollectionSorting.generated.h"

UENUM(BlueprintType)
enum class ESortDirection : uint8
{
	ASCENDING UMETA(DisplayName = "Ascending"),
	DESCENDING UMETA(DisplayName = "Descending")
};

UENUM(BlueprintType)
enum class ESortAxisOrder : uint8
{
	AXIS_X_Y_Z UMETA(DisplayName = "X Y Z"),
	AXIS_Y_X_Z UMETA(DisplayName = "Y X Z"),
	AXIS_Z_X_Y UMETA(DisplayName = "Z X Y"),
	AXIS_X_Z_Y UMETA(DisplayName = "X Z Y"),
	AXIS_Y_Z_X UMETA(DisplayName = "Y Z X"),
	AXIS_Z_Y_X UMETA(DisplayName = "Z Y X"),
	AXIS_LENGTH UMETA(DisplayName = "length"),
};

/**
 * 
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExCollectionSorting : public UPCGDataFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCGEx/Sorting")
	static void SortByDensity(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction = ESortDirection::ASCENDING);
	
	UFUNCTION(BlueprintCallable, Category = "PCGEx/Sorting")
	static void SortByPosition(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction = ESortDirection::ASCENDING, ESortAxisOrder AxisOrder = ESortAxisOrder::AXIS_X_Y_Z);

	UFUNCTION(BlueprintCallable, Category = "PCGEx/Sorting")
	static void SortByScale(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction = ESortDirection::ASCENDING, ESortAxisOrder AxisOrder = ESortAxisOrder::AXIS_X_Y_Z);

	UFUNCTION(BlueprintCallable, Category = "PCGEx/Sorting")
	static void SortByRotation(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction = ESortDirection::ASCENDING, ESortAxisOrder AxisOrder = ESortAxisOrder::AXIS_X_Y_Z);

};
