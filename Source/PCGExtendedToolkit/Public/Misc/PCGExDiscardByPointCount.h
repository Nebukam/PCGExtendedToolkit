// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"

#include "PCGExDiscardByPointCount.generated.h"

namespace PCGExDiscardByPointCount
{
	const FName OutputDiscardedLabel = TEXT("Discarded");
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDiscardByPointCountSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardByPointCount, "Discard By Point Count", "Filter outputs by point count.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Don't output Clusters if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBelow = true;

	/** Discarded if point count is less than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRemoveBelow", ClampMin=0))
	int32 MinPointCount = 1;

	/** Don't output Clusters if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveAbove = false;

	/** Discarded if point count is more than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bRemoveAbove", ClampMin=0))
	int32 MaxPointCount = 500;

	/** Whether or not to allow empty outputs (either discarded or not) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bAllowEmptyOutputs = true;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardByPointCountElement final : public FPCGExPointsProcessorElement
{
	virtual bool Boot(FPCGExContext* InContext) const override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
