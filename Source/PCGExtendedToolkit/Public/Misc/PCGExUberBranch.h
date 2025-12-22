// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGExCompare.h"
#include "Core/PCGExPointsProcessor.h"
#include "PCGExUberBranch.generated.h"

UENUM(BlueprintType)
enum class EPCGExUberBranchMode : uint8
{
	All     = 0 UMETA(DisplayName = "All", ToolTip="All points must pass the filters."),
	Any     = 1 UMETA(DisplayName = "Any", ToolTip="At least one point must pass the filter."),
	Partial = 2 UMETA(DisplayName = "Partial", ToolTip="A given amount of points must pass the filter."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="quality-of-life/uber-branch"))
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
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(FilterHub); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::ControlFlow; }
#endif

	virtual bool OutputPinsCanBeDeactivated() const override { return true; }
	virtual bool HasDynamicPins() const override;

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

	/** Number of collections to check for in parallel. Use 0 to force execution in a single go. Can be beneficial if filters are simple enough. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	int32 AsyncChunkSize = 32;

private:
	friend class FPCGExUberBranchElement;
};

struct FPCGExUberBranchContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExUberBranchElement;

	TArray<int32> Dispatch;
	TArray<TSharedPtr<PCGExPointFilter::FManager>> Managers;
	TArray<TSharedPtr<PCGExData::FFacade>> Facades;
};

class FPCGExUberBranchElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(UberBranch)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};
