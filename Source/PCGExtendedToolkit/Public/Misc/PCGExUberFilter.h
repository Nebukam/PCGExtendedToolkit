// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataFilter.h"

#include "PCGExUberFilter.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExUberFilterSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
#if WITH_EDITOR

public:
#endif
	//~End UObject interface

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(UberFilter, "Uber Filter", "Filter points based on multiple rules & conditions.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorFilter; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Swap Insider & Outside data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSwap = false;

private:
	friend class FPCGExUberFilterElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExUberFilterContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExUberFilterElement;
	virtual ~FPCGExUberFilterContext() override;

	TArray<TObjectPtr<UPCGExFilterDefinitionBase>> FilterDefinitions;
	PCGExDataFilter::TDirectFilterManager* FilterManager = nullptr;

	PCGExData::FPointIOCollection* Inside = nullptr;
	PCGExData::FPointIOCollection* Outside = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExUberFilterElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
