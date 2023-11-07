// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGSettings.h"
#include "Data/PCGExRelationalData.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExRelationalSettingsBase.generated.h"

namespace PCGExRelational
{
	extern const FName SourceLabel;
	extern const FName SourceRelationalParamsLabel;
	extern const FName SourceRelationalDataLabel;
	extern const FName OutputPointsLabel;
	extern const FName OutputRelationalDataLabel;
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExRelationalSettingsBase : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGExRelationalSettingsBase interface
	virtual bool GetRequiresRelationalParams() const { return true; }
	virtual bool GetRequiresRelationalData() const { return false; }
	//~End UPCGExRelationalSettingsBase interface

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("PCGExRelationalSettings")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGExRelationalSettings", "NodeTitle", "Relational Settings"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

private:
	friend class FPCGExRelationalProcessingElementBase;
	friend class UPCGExRelationalData;
};

class FPCGExRelationalProcessingElementBase : public FPCGPointProcessingElementBase
{
public:
	const bool CheckRelationalParams(const FPCGContext* Context) const;
	UPCGExRelationalParamsData* GetRelationalParams(const FPCGContext* Context) const;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	void ExecuteForEachParamsInput(FPCGContext* Context, const TFunction<void(FPCGContext*, UPCGExRelationalParamsData*)>& PerParamsFunc);
	void ExecuteForEachRelationalPairsInput(FPCGContext* Context, const TFunction<void(FPCGContext*, UPCGExRelationalParamsData*)>& PerParamsFunc);

private:
	friend class UPCGExRelationalData;
};
