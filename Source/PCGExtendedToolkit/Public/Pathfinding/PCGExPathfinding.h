// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Relational/PCGExRelationalData.h"
#include "Relational/PCGExRelationalSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExPathfinding.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPathfindingSettings : public UPCGExRelationalSettingsBase
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
	#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("Pathfinding")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExPathfinding", "NodeTitle", "Pathfinding"); }
	virtual FText GetNodeTooltipText() const override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	public:


private:
	friend class FPCGExPathfindingElement;
	
};

class FPCGExPathfindingElement : public FPCGExRelationalProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};