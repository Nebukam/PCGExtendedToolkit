// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPin.h"
#include "Relational/PCGExRelationalData.h"
#include "PCGExRelationalParams.generated.h"

/** Builds a PCGExDirectionalData to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationalParams : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RelationalParams")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExRelationalParams", "NodeTitle", "Relational Params"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:

	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Directions to store a directional relationship. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExRelationsDefinition Slots = {};

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FName RelationalIdentifier = NAME_None;
	
private:
	friend class UPCGExRelationalData;
	
};

class FPCGExRelationalParamsElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	
private:
	friend class UPCGExRelationalData;
};
