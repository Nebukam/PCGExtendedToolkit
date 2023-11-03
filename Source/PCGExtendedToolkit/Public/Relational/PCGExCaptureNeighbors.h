// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "PCGExRelationalData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExCaptureNeighbors.generated.h"

namespace PCGExWriteIndex
{
	extern const FName SourceLabel;
	extern const FName RelationalLabel;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExCaptureNeighbors : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
	#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CaptureNeighbors")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExCaptureNeighbors", "NodeTitle", "Capture Neighbors"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	public:

	/** Sampling radius */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	float CheckExtent = 10000.0f;
	
	/** Whether the point transform affects the sampling direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformAffectDirection = false;
	
	/** Directions to store a directional relationship. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExRelationsDefinition Slots = {};

	/** The name of the attribute to read point index from (or write index to) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IndexAttributeName = TEXT("CurrentIndex");

private:
	friend class FPCGExCaptureNeighborsElement;
	
};

class FPCGExCaptureNeighborsElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};