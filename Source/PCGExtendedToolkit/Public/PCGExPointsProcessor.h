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
#include "RenderGraphResources.h"

#include "PCGExPointsProcessor.generated.h"

struct FPCGExPointsProcessorContext;
class FPCGExPointsProcessorElement;

#pragma region Loops

namespace PCGEx
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FAPointLoop
	{
		virtual ~FAPointLoop() = default;

		FAPointLoop()
		{
		}

		FPCGExPointsProcessorContext* Context = nullptr;

		PCGExData::FPointIO* PointIO = nullptr;

		int32 NumIterations = -1;
		int32 ChunkSize = 32;

		int32 CurrentIndex = -1;
		bool bAsyncEnabled = true;

		inline PCGExData::FPointIO* GetPointIO() const;

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO*)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO*)>&& LoopBody) = 0;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO*)>&& LoopBody) = 0;

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointLoop : public FAPointLoop
	{
		FPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO*)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO*)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO*)>&& LoopBody) override;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FAsyncPointLoop : public FPointLoop
	{
		FAsyncPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(PCGExData::FPointIO*)>&& Initialize, const TFunction<void(const int32, const PCGExData::FPointIO*)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const PCGExData::FPointIO*)>&& LoopBody) override;
	};
}

#pragma endregion

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bDoAsyncProcessing = true;

	/** Async work priority for this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay, EditCondition="bDoAsyncProcessing"))
	EPCGExAsyncPriority WorkPriority = EPCGExAsyncPriority::Default;

	/** Chunk size for parallel processing. <1 switches to preferred node value.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay, EditCondition="bDoAsyncProcessing", ClampMin=-1, ClampMax=8196))
	int32 ChunkSize = -1;

	/** Cache the results of this node. Can yield unexpected result in certain cases.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bCacheResult = false;

	/** Flatten the output of this node.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance", meta=(PCG_NotOverridable, AdvancedDisplay))
	bool bFlattenOutput = false;

protected:
	virtual int32 GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_M; }
	//~End UPCGExPointsProcessorSettings
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPointsProcessorContext : public FPCGExContext
{
	friend class FPCGExPointsProcessorElement;

	virtual ~FPCGExPointsProcessorContext() override;

	UWorld* World = nullptr;

	mutable FRWLock ContextLock;

	PCGExData::FPointIOCollection* MainPoints = nullptr;
	PCGExData::FPointIO* CurrentIO = nullptr;

	virtual bool AdvancePointsIO(const bool bCleanupKeys = true);
	virtual bool ExecuteAutomation();

	void SetAsyncState(const PCGExMT::AsyncState WaitState)
	{
		bIsPaused = true;
		SetState(WaitState, false);
	}

	void SetState(const PCGExMT::AsyncState StateId, const bool bResetAsyncWork = true)
	{
		if (bResetAsyncWork) { ResetAsyncWork(); }
		if (CurrentState == StateId) { return; }
		CurrentState = StateId;
	}

	bool IsState(const PCGExMT::AsyncState StateId) const { return CurrentState == StateId; }

	bool IsSetup() const { return IsState(PCGExMT::State_Setup); }
	bool IsDone() const { return IsState(PCGExMT::State_Done); }

	void Done()
	{
		bUseLock = false;
		SetState(PCGExMT::State_Done);
	}

	bool TryComplete(const bool bForce = false);

	PCGExMT::FTaskManager* GetAsyncManager();

	int32 ChunkSize = 0;
	bool bDoAsyncProcessing = true;

#pragma region Async loops

	template <class InitializeFunc, class LoopBodyFunc>
	bool Process(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop->NumIterations = NumIterations;
		AsyncLoop->bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop->Execute(Initialize, LoopBody);
	}

	template <class LoopBodyFunc>
	bool Process(LoopBodyFunc&& LoopBody, const int32 NumIterations, const bool bForceSync = false)
	{
		AsyncLoop->NumIterations = NumIterations;
		AsyncLoop->bAsyncEnabled = bDoAsyncProcessing && !bForceSync;
		return AsyncLoop->Execute(LoopBody);
	}

	template <typename FullTask>
	void StartAsyncLoop(PCGExData::FPointIO* PointIO, const int32 NumIterations, const int32 ChunkSizeOverride = -1)
	{
		GetAsyncManager()->Start<FullTask>(-1, PointIO, NumIterations, ChunkSizeOverride <= 0 ? ChunkSize : ChunkSizeOverride);
	}

	template <typename T>
	T* MakeLoop()
	{
		T* Loop = new T{};
		Loop->Context = this;
		Loop->ChunkSize = ChunkSize;
		Loop->bAsyncEnabled = bDoAsyncProcessing;
		return Loop;
	}

#pragma endregion

	template <typename T>
	T* RegisterOperation(UPCGExOperation* BaseOperation)
	{
		BaseOperation->BindContext(this); // Temp so Copy doesn't crash
		
		T* RetValue = BaseOperation->CopyOperation<T>();
		OwnedProcessorOperations.Add(RetValue);
		RetValue->BindContext(this);
		return RetValue;
	}

#pragma region Filtering

	TArray<UPCGExFilterFactoryBase*> FilterFactories;

#pragma endregion

#pragma region Batching

	bool ProcessPointsBatch();

	PCGExMT::AsyncState TargetState_PointsProcessingDone;
	PCGExPointsMT::FPointsProcessorBatchBase* MainBatch = nullptr;
	TArray<PCGExData::FPointIO*> BatchablePoints;
	TMap<PCGExData::FPointIO*, PCGExPointsMT::FPointsProcessor*> SubProcessorMap;

	template <typename T, class ValidateEntryFunc, class InitBatchFunc>
	bool StartBatchProcessingPoints(ValidateEntryFunc&& ValidateEntry, InitBatchFunc&& InitBatch, const PCGExMT::AsyncState InState)
	{
		PCGEX_DELETE(MainBatch)

		PCGEX_SETTINGS_LOCAL(PointsProcessor)

		SubProcessorMap.Empty();
		SubProcessorMap.Reserve(MainPoints->Num());

		TargetState_PointsProcessingDone = InState;
		BatchablePoints.Empty();

		while (AdvancePointsIO(false))
		{
			if (!ValidateEntry(CurrentIO)) { continue; }
			BatchablePoints.Add(CurrentIO);
		}

		if (BatchablePoints.IsEmpty()) { return false; }

		MainBatch = new T(this, BatchablePoints);
		MainBatch->SubProcessorMap = &SubProcessorMap;

		T* TypedBatch = static_cast<T*>(MainBatch);
		InitBatch(TypedBatch);

		if (Settings->SupportsPointFilters())
		{
			TypedBatch->SetPointsFilterData(&FilterFactories);
		}

		if (MainBatch->PrepareProcessing()) { MainBatch->Process(GetAsyncManager()); }
		SetAsyncState(PCGExPointsMT::MTState_PointsProcessing);

		return true;
	}

	virtual void MTState_PointsProcessingDone()
	{
	}

	virtual void MTState_PointsCompletingWorkDone()
	{
	}

	virtual void MTState_PointsWritingDone()
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

protected:
	PCGExMT::FAsyncParallelLoop* AsyncLoop = nullptr;
	PCGExMT::FTaskManager* AsyncManager = nullptr;

	PCGExMT::AsyncState CurrentState;
	int32 CurrentPointIOIndex = -1;

	TArray<UPCGExOperation*> ProcessorOperations;
	TSet<UPCGExOperation*> OwnedProcessorOperations;

	virtual void ResetAsyncWork();

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

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override
	{
		const UPCGExPointsProcessorSettings* Settings = static_cast<const UPCGExPointsProcessorSettings*>(InSettings);
		return Settings->bCacheResult;
	}

	virtual void DisabledPassThroughData(FPCGContext* Context) const override;

protected:
	virtual FPCGContext* InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	virtual bool Boot(FPCGExContext* InContext) const;
};
