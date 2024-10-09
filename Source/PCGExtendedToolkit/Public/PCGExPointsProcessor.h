// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Runtime/Launch/Resources/Version.h"

#include "CoreMinimal.h"
#include "PCGPin.h"

#include "PCGEx.h"
#include "PCGExContext.h"
#include "PCGExMT.h"
#include "PCGExMacros.h"
#include "Data/PCGExPointIO.h"
#include "PCGExOperation.h"
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

struct FPCGExPointsProcessorContext;
class FPCGExPointsProcessorElement;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend struct FPCGExPointsProcessorContext;
	friend class FPCGExPointsProcessorElement;

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual bool OnlyPassThroughOneEdgeWhenDisabled() const override { return false; }
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool IsInputless() const { return false; }

	virtual FName GetMainInputLabel() const { return PCGEx::SourcePointsLabel; }
	virtual FName GetMainOutputLabel() const { return PCGEx::OutputPointsLabel; }
	virtual bool GetMainAcceptMultipleData() const { return true; }
	virtual PCGExData::EInit GetMainOutputInitMode() const;

	virtual FName GetPointFilterLabel() const { return NAME_None; }
	virtual FString GetPointFilterTooltip() const { return TEXT("Filters"); }
	virtual TSet<PCGExFactories::EType> GetPointFilterTypes() const { return PCGExFactories::PointFilters; }
	virtual bool RequiresPointFilters() const { return false; }

	bool SupportsPointFilters() const { return !GetPointFilterLabel().IsNone(); }

	/** Forces execution on main thread. Work is still chunked. Turning this off ensure linear order of operations, and, in most case, determinism.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bDoAsyncProcessing = true;

	/** Async work priority for this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay, EditCondition="bDoAsyncProcessing"))
	EPCGExAsyncPriority WorkPriority = EPCGExAsyncPriority::Default;

	/** Chunk size for parallel processing. <1 switches to preferred node value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay, EditCondition="bDoAsyncProcessing", ClampMin=-1, ClampMax=8196))
	int32 ChunkSize = -1;

	/** Cache the results of this node. Can yield unexpected result in certain cases.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bCacheResult = false;

	/** Flatten the output of this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bFlattenOutput = false;

	/** Whether scoped attribute read is enabled or not. Disabling this on small dataset may greatly improve performance. It's enabled by default for legacy reasons. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Performance, meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bScopedAttributeGet = true;

protected:
	virtual int32 GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }
	//~End UPCGExPointsProcessorSettings
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointsProcessorContext : FPCGExContext
{
	friend class FPCGExPointsProcessorElement;

	bool bScopedAttributeGet = false;
	virtual ~FPCGExPointsProcessorContext() override;

	mutable FRWLock AsyncLock;

	TSharedPtr<PCGExData::FPointIOCollection> MainPoints;
	TSharedPtr<PCGExData::FPointIO> CurrentIO;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true);

	TSharedPtr<PCGExMT::FTaskManager> GetAsyncManager();

	int32 ChunkSize = 0;

	template <typename T>
	T* RegisterOperation(UPCGExOperation* BaseOperation)
	{
		BaseOperation->BindContext(this); // Temp so Copy doesn't crash

		T* RetValue = BaseOperation->CopyOperation<T>();
		OwnedProcessorOperations.Add(RetValue);
		RetValue->FindSettingsOverrides(this);
		return RetValue;
	}

#pragma region Filtering

	TArray<TObjectPtr<const UPCGExFilterFactoryBase>> FilterFactories;

#pragma endregion

#pragma region Batching

	bool bBatchProcessingEnabled = false;
	bool ProcessPointsBatch(const PCGEx::AsyncState NextStateId, const bool bIsNextStateAsync = false);

	TSharedPtr<PCGExPointsMT::FPointsProcessorBatchBase> MainBatch;
	TMap<PCGExData::FPointIO*, TSharedRef<PCGExPointsMT::FPointsProcessor>> SubProcessorMap;

	template <typename T, class ValidateEntryFunc, class InitBatchFunc>
	bool StartBatchProcessingPoints(ValidateEntryFunc&& ValidateEntry, InitBatchFunc&& InitBatch)
	{
		bBatchProcessingEnabled = false;

		MainBatch.Reset();

		PCGEX_SETTINGS_LOCAL(PointsProcessor)

		SubProcessorMap.Empty();
		SubProcessorMap.Reserve(MainPoints->Num());

		TArray<TWeakPtr<PCGExData::FPointIO>> BatchAblePoints;
		BatchAblePoints.Reserve(MainPoints->Pairs.Num());

		while (AdvancePointsIO(false))
		{
			if (!ValidateEntry(CurrentIO)) { continue; }
			BatchAblePoints.Add(CurrentIO.ToSharedRef());
		}

		if (BatchAblePoints.IsEmpty()) { return bBatchProcessingEnabled; }
		bBatchProcessingEnabled = true;

		TSharedPtr<T> TypedBatch = MakeShared<T>(this, BatchAblePoints);

		MainBatch = TypedBatch;
		MainBatch->SubProcessorMap = &SubProcessorMap;

		InitBatch(TypedBatch);

		if (Settings->SupportsPointFilters()) { TypedBatch->SetPointsFilterData(&FilterFactories); }

		if (MainBatch->PrepareProcessing())
		{
			SetAsyncState(PCGExPointsMT::MTState_PointsProcessing);
			MainBatch->Process(GetAsyncManager());
		}
		else
		{
			bBatchProcessingEnabled = false;
		}

		return bBatchProcessingEnabled;
	}

	virtual void BatchProcessing_InitialProcessingDone()
	{
	}

	virtual void BatchProcessing_WorkComplete()
	{
	}

	virtual void BatchProcessing_WritingDone()
	{
	}

	template <typename T>
	void GatherProcessors(TArray<T*> OutProcessors)
	{
		OutProcessors.Reserve(MainBatch->GetNumProcessors());
		PCGExPointsMT::TBatch<T>* TypedBatch = static_cast<PCGExPointsMT::TBatch<T>*>(MainBatch);
		OutProcessors.Append(TypedBatch->Processors);
	}

#pragma endregion

	virtual bool ShouldWaitForAsync() override;

protected:
	TSharedPtr<PCGExMT::FTaskManager> AsyncManager;

	int32 CurrentPointIOIndex = -1;

	TArray<UPCGExOperation*> ProcessorOperations;
	TSet<UPCGExOperation*> OwnedProcessorOperations;

	virtual void ResumeExecution() override;

public:
	virtual bool IsAsyncWorkComplete();
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointsProcessorElement : public IPCGElement
{
public:
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;

#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override;
	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual FPCGExContext* InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	virtual bool Boot(FPCGExContext* InContext) const;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const;
	virtual bool PostBoot(FPCGExContext* InContext) const;
};
