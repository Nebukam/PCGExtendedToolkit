// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "PCGEx.h"
#include "Details/PCGExMacros.h"

#include "PCGExContext.h"
#include "PCGExGlobalSettings.h" // Needed for child classes
#include "PCGExPointsMT.h"

#include "PCGExPointsProcessor.generated.h"

#define PCGEX_EXECUTION_CHECK_C(_CONTEXT) if(!_CONTEXT->CanExecute()){ return true; } if (!_CONTEXT->IsAsyncWorkComplete()) { return false; }
#define PCGEX_EXECUTION_CHECK PCGEX_EXECUTION_CHECK_C(Context)
#define PCGEX_ASYNC_WAIT_C(_CONTEXT) if (_CONTEXT->bWaitingForAsyncCompletion) { return false; }
#define PCGEX_ASYNC_WAIT PCGEX_ASYNC_WAIT_C(Context)
#define PCGEX_ASYNC_WAIT_INTERNAL if (bWaitingForAsyncCompletion) { return false; }
#define PCGEX_ON_STATE(_STATE) if(Context->IsState(_STATE))
#define PCGEX_ON_STATE_INTERNAL(_STATE) if(IsState(_STATE))
#define PCGEX_ON_ASYNC_STATE_READY(_STATE) if(Context->IsState(_STATE) && Context->ShouldWaitForAsync()){ return false; }else if(Context->IsState(_STATE) && !Context->ShouldWaitForAsync())
#define PCGEX_ON_ASYNC_STATE_READY_INTERNAL(_STATE) if(const bool ShouldWait = ShouldWaitForAsync(); IsState(_STATE) && ShouldWait){ return false; }else if(IsState(_STATE) && !ShouldWait)
#define PCGEX_ON_INITIAL_EXECUTION if(Context->IsInitialExecution())
#define PCGEX_POINTS_BATCH_PROCESSING(_STATE) if (!Context->ProcessPointsBatch(_STATE)) { return false; }

#define PCGEX_ELEMENT_CREATE_CONTEXT(_CLASS) virtual FPCGContext* CreateContext() override { return new FPCGEx##_CLASS##Context(); }
#define PCGEX_ELEMENT_CREATE_DEFAULT_CONTEXT virtual FPCGContext* CreateContext() override { return new FPCGExContext(); }

#define PCGEX_ELEMENT_BATCH_POINT_DECL virtual TSharedPtr<PCGExPointsMT::IBatch> CreatePointBatchInstance(const TArray<TWeakPtr<PCGExData::FPointIO>>& InData) const override;
#define PCGEX_ELEMENT_BATCH_POINT_IMPL(_CLASS) TSharedPtr<PCGExPointsMT::IBatch> FPCGEx##_CLASS##Context::CreatePointBatchInstance(const TArray<TWeakPtr<PCGExData::FPointIO>>& InData) const{ \
return MakeShared<PCGExPointsMT::TBatch<PCGEx##_CLASS::FProcessor>>(const_cast<FPCGEx##_CLASS##Context*>(this), InData); }
#define PCGEX_ELEMENT_BATCH_POINT_IMPL_ADV(_CLASS) TSharedPtr<PCGExPointsMT::IBatch> FPCGEx##_CLASS##Context::CreatePointBatchInstance(const TArray<TWeakPtr<PCGExData::FPointIO>>& InData) const{ \
return MakeShared<PCGEx##_CLASS::FBatch>(const_cast<FPCGEx##_CLASS##Context*>(this), InData); }

class UPCGExPointFilterFactoryData;

namespace PCGExPointsMT
{
	class IProcessor;
	class IBatch;
}

namespace PCGExData
{
	class FPointIO;
	class FPointIOCollection;
}

class UPCGExInstancedFactory;

namespace PCGExFactories
{
	enum class EType : uint8;
}

struct FPCGExPointsProcessorContext;
class FPCGExPointsProcessorElement;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend struct FPCGExPointsProcessorContext;
	friend class FPCGExPointsProcessorElement;

public:
	//~Begin UPCGSettings	
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::PointOps; }

	virtual bool GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
#endif

protected:
	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool OnlyPassThroughOneEdgeWhenDisabled() const override { return false; }
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool IsInputless() const { return false; }

	virtual FName GetMainInputPin() const { return PCGPinConstants::DefaultInputLabel; }
	virtual FName GetMainOutputPin() const { return PCGPinConstants::DefaultOutputLabel; }
	virtual bool GetMainAcceptMultipleData() const { return true; }
	virtual bool GetIsMainTransactional() const { return false; }

	virtual PCGExData::EIOInit GetMainOutputInitMode() const;

	virtual FName GetPointFilterPin() const { return NAME_None; }
	virtual FString GetPointFilterTooltip() const { return TEXT("Filters"); }
	virtual TSet<PCGExFactories::EType> GetPointFilterTypes() const;
	virtual bool RequiresPointFilters() const { return false; }

	bool SupportsPointFilters() const { return !GetPointFilterPin().IsNone(); }

	/** If enabled, will pre-instanciate all data on a single thread to avoid contention. Not all nodes support this. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	EPCGExOptionState BulkInitData = EPCGExOptionState::Default;

	/** Async work priority for this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	EPCGExAsyncPriority WorkPriority = EPCGExAsyncPriority::Default;

	/** Cache the results of this node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	EPCGExOptionState CacheData = EPCGExOptionState::Default;

	/** Flatten the output of this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bFlattenOutput = false;

	/** Whether scoped attribute read is enabled or not. Disabling this on small dataset may greatly improve performance. It's enabled by default for legacy reasons. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	EPCGExOptionState ScopedAttributeGet = EPCGExOptionState::Default;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bPropagateAbortedExecution = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietInvalidInputWarning = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietMissingInputError = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bQuietCancellationError = false;

	//~End UPCGExPointsProcessorSettings

#if WITH_EDITOR
	/** Open a browser and navigate to that node' documentation page. */
	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Node Documentation", ShortToolTip="Open a browser and navigate to that node' documentation page", DisplayOrder=-1))
	void EDITOR_OpenNodeDocumentation() const;
#endif

protected:
	virtual bool ShouldCache() const;
	virtual bool WantsScopedAttributeGet() const;
	virtual bool WantsBulkInitData() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : FPCGExContext
{
	friend class FPCGExPointsProcessorElement;

	using FBatchProcessingValidateEntry = std::function<bool(const TSharedPtr<PCGExData::FPointIO>&)>;
	using FBatchProcessingInitPointBatch = std::function<void(const TSharedPtr<PCGExPointsMT::IBatch>&)>;

	virtual ~FPCGExPointsProcessorContext() override;

	TSharedPtr<PCGExData::FPointIOCollection> MainPoints;
	TSharedPtr<PCGExData::FPointIO> CurrentIO;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true);

	int32 InitialMainPointsNum = 0;

	UPCGExInstancedFactory* RegisterOperation(UPCGExInstancedFactory* BaseOperation, const FName OverridePinLabel = NAME_None);


#pragma region Filtering

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> FilterFactories;

#pragma endregion

#pragma region Batching

	bool bBatchProcessingEnabled = false;
	bool ProcessPointsBatch(const PCGExCommon::ContextState NextStateId, const bool bIsNextStateAsync = false);

	TSharedPtr<PCGExPointsMT::IBatch> MainBatch;
	TMap<PCGExData::FPointIO*, TSharedRef<PCGExPointsMT::IProcessor>> SubProcessorMap;

	bool StartBatchProcessingPoints(FBatchProcessingValidateEntry&& ValidateEntry, FBatchProcessingInitPointBatch&& InitBatch);

	virtual void BatchProcessing_InitialProcessingDone();
	virtual void BatchProcessing_WorkComplete();
	virtual void BatchProcessing_WritingDone();

#pragma endregion

protected:
	int32 CurrentPointIOIndex = -1;

	virtual TSharedPtr<PCGExPointsMT::IBatch> CreatePointBatchInstance(const TArray<TWeakPtr<PCGExData::FPointIO>>& InData) const
	PCGEX_NOT_IMPLEMENTED_RET(CreatePointBatchInstance, nullptr);

	TArray<UPCGExInstancedFactory*> ProcessorOperations;
	TSet<UPCGExInstancedFactory*> InternalOperations;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElement : public IPCGElement
{
public:
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;

	virtual FPCGContext* Initialize(const FPCGInitializeElementParams& InParams) override;

#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual FPCGContext* CreateContext() override;

	virtual void OnContextInitialized(FPCGExPointsProcessorContext* InContext) const;

	virtual bool Boot(FPCGExContext* InContext) const;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const;
	virtual bool PostBoot(FPCGExContext* InContext) const;
	virtual void AbortInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override;
};
