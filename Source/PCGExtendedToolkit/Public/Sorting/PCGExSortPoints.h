// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPointSortHelpers.h"
#include "PCGExSortPoints.generated.h"

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
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SortPoints")); }
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

	/** The point property to sample and drive the sort. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGPointProperties SortOver = EPCGPointProperties::Density;

	/** Controls the order in which points will be ordered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	ESortDirection SortDirection = ESortDirection::Ascending;

	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	ESortAxisOrder SortOrder = ESortAxisOrder::Axis_X_Y_Z;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Tolerance = 0.0001f;
	
private:
	friend class FPCGExSortPointsElement;
	
};

class FPCGExSortPointsElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};