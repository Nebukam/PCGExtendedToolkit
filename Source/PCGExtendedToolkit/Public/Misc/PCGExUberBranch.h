// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"


#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"


#include "PCGExUberBranch.generated.h"

UENUM(BlueprintType)
enum class EPCGExUberBranchMode : uint8
{
	All     = 0 UMETA(DisplayName = "All", ToolTip="All points must pass the filters."),
	Any     = 1 UMETA(DisplayName = "Any", ToolTip="At least one point must pass the filter."),
	Partial = 2 UMETA(DisplayName = "Partial", ToolTip="A given amount of points must pass the filter."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="filters/uber-filter-collection"))
class UPCGExUberBranchSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(UberBranch, "Uber Branch", "Branch collections based on multiple rules & conditions.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorFilterHub); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::ControlFlow; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputPin() const override;
	//~End UPCGExPointsProcessorSettings

	/** Write result to point instead of split outputs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 NumBranches = 3;

	UPROPERTY(meta=(PCG_NotOverridable))
	TArray<FName> InputLabels = {TEXT("→ 0"), TEXT("→ 1"), TEXT("→ 2")};

	UPROPERTY(meta=(PCG_NotOverridable))
	TArray<FName> OutputLabels = {TEXT("0 →"), TEXT("1 →"), TEXT("2 →")};

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietMissingFilters = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietInvalidFilters = false;
	
private:
	friend class FPCGExUberBranchElement;
};

struct FPCGExUberBranchContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExUberBranchElement;

	TArray<TArray<TObjectPtr<const UPCGExFilterFactoryData>>> Filters;
	TArray<TSharedPtr<PCGExPointFilter::FManager>> Managers;
	TArray<TSharedPtr<PCGExData::FFacade>> Facades;

};

class FPCGExUberBranchElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(UberBranch)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

