// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExRelationsParamsProcessor.generated.h"

class UPCGExRelationsParamsData;

namespace PCGExRelational
{
	extern const FName SourceLabel;
	extern const FName SourceRelationalParamsLabel;
	extern const FName OutputPointsLabel;
}

/**
 * A Base node to process a set of point using RelationalParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("RelationsProcessorSettings")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExRelationsProcessorSettings", "NodeTitle", "Relations Processor Settings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExRelationsProcessorElement;
	friend class UPCGExRelationsData;
};

class FPCGExRelationsProcessorElement : public FPCGPointProcessingElementBase
{
public:
	const bool CheckRelationalParams(const FPCGContext* Context) const;
	UPCGExRelationsParamsData* GetRelationalParams(const FPCGContext* Context) const;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	void ExecuteForEachParamsInput(FPCGContext* Context, const TFunction<void(FPCGContext*, UPCGExRelationsParamsData*)>& PerParamsFunc);

private:
	friend class UPCGExRelationsData;
};
