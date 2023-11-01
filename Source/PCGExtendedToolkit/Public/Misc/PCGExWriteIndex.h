// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

/*
 * This is a dummy class to create new simple PCG nodes
 */

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExWriteIndex.generated.h"

namespace PCGExWriteIndex
{
	extern const FName SourceLabel;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExWriteIndexSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("WriteIndex")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExWriteIndex", "NodeTitle", "Write Index"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:

	/** The name of the attribute to store on the point. Use 'None' to disable */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName AttributeName = TEXT("CurrentIndex");
	
};

class FPCGExWriteIndexElement : public FPCGPointProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};