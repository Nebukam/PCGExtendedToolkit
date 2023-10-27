// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPointDataSorting.h"
#include "PCGExSortPoints.generated.h"

UENUM(BlueprintType)
enum class ESortDataSource : uint8
{
	SOURCE_DENSITY UMETA(DisplayName = "Density"),
	SOURCE_STEEPNESS UMETA(DisplayName = "Steepness"),
	SOURCE_POSITION UMETA(DisplayName = "Position"),
	SOURCE_SCALE UMETA(DisplayName = "Scale"),
};

namespace PCGExSortPoints
{
	extern const FName SourceLabel;	
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExSortPointsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Sort Points")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExSortPoints", "NodeTitle", "Sort Points"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	/** The point property to sample and drive the sort */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	ESortDataSource SortOver = ESortDataSource::SOURCE_DENSITY;

	/** Controls the order in which points will be ordered */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	ESortDirection SortDirection = ESortDirection::ASCENDING;

	/** Sub-sorting order, used only for vector properties. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	ESortAxisOrder SortOrder = ESortAxisOrder::AXIS_X_Y_Z;
	
};

class FPCGExSortPointsElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};