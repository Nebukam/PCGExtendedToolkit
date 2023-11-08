// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExRelationsHelpers.h"
#include "PCGExRelationsParamsProcessor.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExFindRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExFindRelationsSettings : public UPCGExRelationsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FindRelations")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExFindRelations", "NodeTitle", "Find Relations"); }
	virtual FText GetNodeTooltipText() const override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:


private:
	friend class FPCGExFindRelationsElement;
};

class FPCGExFindRelationsElement : public FPCGExRelationsProcessorElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
