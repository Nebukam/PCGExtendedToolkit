// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Elements/PCGExecuteBlueprint.h"
#include "PCGPin.h"
#include "PCGExCollectionSorting.h"
#include "PCGExSortPointsByAttribute.generated.h"

/**
 *
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsByAttribute : public UPCGBlueprintElement
{
	GENERATED_BODY()

public:
	UPCGExSortPointsByAttribute();
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

	/**
	 * Please add a function description
	 *
	 * NOTE: This function is linked to BlueprintImplementableEvent: UPCGBlueprintElement::PointLoopBody
	 */
	virtual bool PointLoopBody_Implementation(const FPCGContext& InContext, const UPCGPointData* InData, const FPCGPoint& InPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;

public:

	const FName NODE_NAME = FName(TEXT("PCGEx | SortPointsByAttribute"));
	const FName NAME_SOURCE_POINTS = FName(TEXT("In Points"));
	const FName NAME_OUT_POINTS = FName(TEXT("Out Points"));

	/** Please add a variable description */
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Default")
	FPCGPoint StartPoint;

protected:
	/** Input pins **/
	FPCGPinProperties InputPinPoints;
	/** Output pins **/
	FPCGPinProperties OutputPinPoints;

};
