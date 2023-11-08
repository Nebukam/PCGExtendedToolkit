// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGExRelationsHelpers.h"
#include "PCGSettings.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGExRelationsParamsProcessor.generated.h"

class UPCGExRelationsParamsData;

namespace PCGExRelational
{
	extern const FName SourcePointsLabel;
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

class FPCGExRelationsProcessorContext : public FPCGContext
{
	friend class FPCGExRelationsProcessorElement;

public:
	PCGExRelational::FParamsInputs Params;
	FPCGExPointIOMap<FPCGExIndexedPointDataIO> Points;

	int32 GetCurrentParamsIndex() const { return CurrentParamsIndex; };
	UPCGExRelationsParamsData* GetCurrentParams() { return Params.Params[CurrentParamsIndex]; }

	bool AdvanceParams(bool bResetPointsIndex = true)
	{
		CurrentParamsIndex++;
		if (Params.Params.IsValidIndex(CurrentParamsIndex))
		{
			if (bResetPointsIndex) { CurrentPointsIndex = -1; }
			return true;
		}
		return false;
	}

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	FPCGExIndexedPointDataIO* GetCurrentPointsIO() { return &Points.Pairs[CurrentPointsIndex]; }

	bool AdvancePointsIO()
	{
		CurrentPointsIndex++;
		if (Points.Pairs.IsValidIndex(CurrentParamsIndex)) { return true; }
		return false;
	}

	void Reset()
	{
		CurrentParamsIndex = -1;
		CurrentParamsIndex = -1;
	}

protected:
	int32 CurrentParamsIndex = -1;
	int32 CurrentPointsIndex = -1;
	virtual bool InitializePointsOutput() const { return true; }
	
};

class FPCGExRelationsProcessorElement : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

private:
	friend class UPCGExRelationsData;
};
