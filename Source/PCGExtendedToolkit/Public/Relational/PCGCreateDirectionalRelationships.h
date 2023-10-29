// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "RelationalDataTypes.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGCreateDirectionalRelationships.generated.h"

namespace PCGExWriteIndex
{
	extern const FName SourceLabel;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGDirectionalRelationships : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
	#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("CreateDirRel")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGDirectionalRelationships", "NodeTitle", "Compute Directional Relationship"); }
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
	
	/** Slots to store a directional relationship. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FDRSlotListSettings Slots = {};

	/** The name of the attribute to read point index from (or write index to) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName IndexAttributeName = TEXT("Index");
	
};

class FPCGDirectionalRelationships : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};