// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGSettings.h"
#include "Data/PCGExDataCommon.h"
#include "PCGExSettings.generated.h"

class UPCGExInstancedFactory;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXCORE_API UPCGExSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend struct FPCGExContext;
	friend class IPCGExElement;

public:
	//~Begin UPCGSettings	
#if WITH_EDITOR
	virtual bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const override;
#endif

	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;

protected:
	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const;
	virtual bool OnlyPassThroughOneEdgeWhenDisabled() const override { return false; }
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	/** If enabled, will pre-allocate all data on a single thread to avoid contention. Not all nodes support this. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable))
	EPCGExOptionState BulkInitData = EPCGExOptionState::Default;

	/** Cache the results of this node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable))
	EPCGExOptionState CacheData = EPCGExOptionState::Default;

	/** Whether scoped attribute read is enabled or not. Disabling this on small dataset may greatly improve performance. It's enabled by default for legacy reasons. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable))
	EPCGExOptionState ScopedAttributeGet = EPCGExOptionState::Default;

	/** Flatten the output of this node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cleanup", meta=(PCG_NotOverridable))
	bool bFlattenOutput = false;

	/** If the node registers consumable attributes, these will be deleted from the output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cleanup", meta=(PCG_NotOverridable))
	bool bCleanupConsumableAttributes = false;

	/** If the node registers consumable attributes, this a list of comma separated names that won't be deleted if they were registered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cleanup", meta=(PCG_Overridable, DisplayName="Protected Attributes", EditCondition="bCleanupConsumableAttributes"))
	FString CommaSeparatedProtectedAttributesName;

	/** Hardcoded set for ease of use. Not mutually exclusive with the overridable string, just easier to edit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cleanup", meta=(PCG_NotOverridable, DisplayName="Protected Attributes", EditCondition="bCleanupConsumableAttributes"))
	TArray<FName> ProtectedAttributes;

	/** Whether the execution of the graph should be cancelled if this node execution is cancelled internally */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bPropagateAbortedExecution = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietInvalidInputWarning = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietMissingInputError = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable))
	bool bQuietCancellationError = false;

	//~End UPCGExPointsProcessorSettings

#if WITH_EDITOR
	/** Open a browser and navigate to that node' documentation page. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Node Documentation", ShortToolTip="Open a browser and navigate to that node' documentation page", DisplayOrder=-1))
	void EDITOR_OpenNodeDocumentation() const;
#endif

protected:
	/** Store version of the node, used for deprecation purposes */
	UPROPERTY()
	int64 PCGExDataVersion = -1;

	virtual bool ShouldCache() const;
	virtual bool WantsScopedAttributeGet() const;
	virtual bool WantsBulkInitData() const;
};
