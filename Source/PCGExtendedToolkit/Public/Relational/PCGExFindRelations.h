// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExRelationalData.h"
#include "PCGExRelationalSettingsBase.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExFindRelations.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExFindRelationsSettings : public UPCGExRelationalSettingsBase
{
	GENERATED_BODY()

public:
	//~Begin UPCGExRelationalSettingsBase interface
	virtual bool GetRequiresRelationalParams() const override { return true; };
	virtual bool GetRequiresRelationalData() const override { return false; };
	//~End UPCGExRelationalSettingsBase interface

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("FindRelations")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExFindRelations", "NodeTitle", "Find Relations"); }
	virtual FText GetNodeTooltipText() const override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:


private:
	friend class FPCGExFindRelationsElement;
};

class FPCGExFindRelationsElement : public FPCGExRelationalProcessingElementBase
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
