// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Elements/PCGExecuteBlueprint.h"
#include "PCGPin.h"
#include "PCGExPointDataSorting.h"
#include "PCGExSortPointsElement.generated.h"

UENUM(BlueprintType)
enum class ESortDataSource : uint8
{
	SOURCE_DENSITY UMETA(DisplayName = "Density"),
	SOURCE_STEEPNESS UMETA(DisplayName = "Steepness"),
	SOURCE_POSITION UMETA(DisplayName = "Position"),
	SOURCE_SCALE UMETA(DisplayName = "Scale"),
};

/**
 *
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsElement : public UPCGBlueprintElement
{
	GENERATED_BODY()

public:
	UPCGExSortPointsElement();
	/**
	 * Override for the default node name
	 *
	 * NOTE: This function is linked to BlueprintNativeEvent: UPCGBlueprintElement::NodeTitleOverride
	 */
	virtual FName NodeTitleOverride_Implementation() const { return NODE_NAME; }

	/**
	 * ~End UObject interface
	 *
	 * NOTE: This function is linked to BlueprintNativeEvent: UPCGBlueprintElement::ExecuteWithContext
	 */
	virtual void ExecuteWithContext_Implementation(UPARAM(ref) FPCGContext& InContext, const FPCGDataCollection& Input, FPCGDataCollection& Output);

public:

	const FName NODE_NAME = FName(TEXT("PCGEx | SortPoints"));
	const FName NAME_SOURCE_POINTS = FName(TEXT("In Points"));
	const FName NAME_OUT_POINTS = FName(TEXT("Out Points"));

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	ESortDataSource SortOver;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	ESortDirection SortDirection;

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	ESortAxisOrder SortOrder;

protected:
	/** Input pins **/
	FPCGPinProperties InPinPoints;
	/** Output pins **/
	FPCGPinProperties OutPinPoints;

};
