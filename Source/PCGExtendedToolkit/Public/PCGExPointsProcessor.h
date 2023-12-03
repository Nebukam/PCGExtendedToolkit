// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "Elements/PCGPointProcessingElementBase.h"
#include "PCGEx.h"
#include "PCGExMT.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"
#include "PCGExInstruction.h"

#include "PCGExPointsProcessor.generated.h"

struct FPCGExPointsProcessorContext;
class FPointTask;

namespace PCGEx
{
	struct PCGEXTENDEDTOOLKIT_API FAPointLoop
	{
		virtual ~FAPointLoop() = default;

		FAPointLoop()
		{
		}

		FPCGExPointsProcessorContext* Context = nullptr;

		UPCGExPointIO* PointIO = nullptr;

		int32 NumIterations = -1;
		int32 ChunkSize = 32;

		int32 CurrentIndex = -1;
		bool bAsyncEnabled = true;

		inline UPCGExPointIO* GetPointIO();

		virtual bool Advance(const TFunction<void(UPCGExPointIO*)>&& Initialize, const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) = 0;
		virtual bool Advance(const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) = 0;

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointLoop : public FAPointLoop
	{
		FPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(UPCGExPointIO*)>&& Initialize, const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FBulkPointLoop : public FPointLoop
	{
		FBulkPointLoop()
		{
		}

		TArray<FPointLoop> SubLoops;

		virtual void Init();
		virtual bool Advance(const TFunction<void(UPCGExPointIO*)>&& Initialize, const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FAsyncPointLoop : public FPointLoop
	{
		FAsyncPointLoop()
		{
		}

		virtual bool Advance(const TFunction<void(UPCGExPointIO*)>&& Initialize, const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
	};

	struct PCGEXTENDEDTOOLKIT_API FBulkAsyncPointLoop : public FAsyncPointLoop
	{
		FBulkAsyncPointLoop()
		{
		}

		TArray<FAsyncPointLoop> SubLoops;

		virtual void Init();
		virtual bool Advance(const TFunction<void(UPCGExPointIO*)>&& Initialize, const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
		virtual bool Advance(const TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody) override;
	};
}


/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCGEXTENDEDTOOLKIT_API UPCGExPointsProcessorSettings : public UPCGSettings
{
	GENERATED_BODY()

	friend struct FPCGExPointsProcessorContext;
	friend class FPCGExPointsProcessorElementBase;

public:
	UPCGExPointsProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PointsProcessorSettings, "Points Processor Settings", "TOOLTIP_TEXT");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	//~End UPCGSettings interface

	virtual FName GetMainPointsInputLabel() const;
	virtual FName GetMainPointsOutputLabel() const;
	virtual PCGExPointIO::EInit GetPointOutputInitMode() const;

	/** Forces execution on main thread. Work is still chunked. Turning this off ensure linear order of operations, and, in most case, determinism.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	bool bDoAsyncProcessing = true;

	/** Chunk size for parallel processing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Performance")
	int32 ChunkSize = -1;

	template <typename T>
	static T* EnsureInstruction(UPCGExInstruction* Instruction) { return Instruction ? static_cast<T*>(Instruction) : NewObject<T>(); }

	template <typename T>
	static T* EnsureInstruction(UPCGExInstruction* Instruction, FPCGExPointsProcessorContext* InContext)
	{
		T* RetValue = Instruction ? static_cast<T*>(Instruction) : NewObject<T>();
		RetValue->BindContext(InContext);
		return RetValue;
	}

protected:
	virtual int32 GetPreferredChunkSize() const;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorContext : public FPCGContext
{
	friend class FPCGExPointsProcessorElementBase;
	friend class FPointTask;

public:
	mutable FRWLock ContextLock;
	UPCGExPointIOGroup* MainPoints = nullptr;

	int32 GetCurrentPointsIndex() const { return CurrentPointsIndex; };
	UPCGExPointIO* CurrentIO = nullptr;

	bool AdvancePointsIO();
	UWorld* World = nullptr;

	PCGExMT::AsyncState GetState() const { return CurrentState; }
	bool IsState(const PCGExMT::AsyncState OperationId) const { return CurrentState == OperationId; }
	bool IsSetup() const { return IsState(PCGExMT::State_Setup); }
	bool IsDone() const { return IsState(PCGExMT::State_Done); }
	void Done() { SetState(PCGExMT::State_Done); }

	virtual void SetState(PCGExMT::AsyncState OperationId);
	virtual void Reset();

	virtual bool ValidatePointDataInput(UPCGPointData* PointData);
	virtual void PostInitPointDataInput(UPCGExPointIO* PointData);

	int32 ChunkSize = 0;
	bool bDoAsyncProcessing = true;

	void OutputPoints() { MainPoints->OutputTo(this); }

	bool BulkProcessMainPoints(TFunction<void(UPCGExPointIO*)>&& Initialize, TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody);
	bool ProcessCurrentPoints(TFunction<void(UPCGExPointIO*)>&& Initialize, TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody, bool bForceSync = false);
	bool ProcessCurrentPoints(TFunction<void(const int32, const UPCGExPointIO*)>&& LoopBody, bool bForceSync = false);

	template <typename T>
	T MakePointLoop()
	{
		T Loop = T{};
		Loop.Context = this;
		Loop.ChunkSize = ChunkSize;
		Loop.bAsyncEnabled = bDoAsyncProcessing;
		return Loop;
	}

protected:
	mutable FRWLock AsyncCreateLock;
	mutable FRWLock AsyncUpdateLock;

	PCGEx::FPointLoop ChunkedPointLoop;
	PCGEx::FAsyncPointLoop AsyncPointLoop;
	PCGEx::FBulkAsyncPointLoop BulkAsyncPointLoop;

	PCGExMT::AsyncState CurrentState = PCGExMT::State_Setup;
	int32 CurrentPointsIndex = -1;

	int32 NumAsyncTaskStarted = 0;
	int32 NumAsyncTaskCompleted = 0;

	virtual void ResetAsyncWork();

	template <typename T>
	FAsyncTask<T>* CreateTask(const int32 Index, const PCGMetadataEntryKey Key, const int32 Attempt = 0)
	{
		FAsyncTask<T>* AsyncTask = new FAsyncTask<T>(this, CurrentIO, PCGExMT::FTaskInfos(Index, Key, Attempt));
		return AsyncTask;
	}

	template <typename T>
	void CreateAndStartTask(const int32 Index, const PCGMetadataEntryKey Key, const int32 Attempt = 0)
	{
		FAsyncTask<T>* AsyncTask = new FAsyncTask<T>(this, CurrentIO, PCGExMT::FTaskInfos(Index, Key, Attempt));
		StartTask(AsyncTask);
	}

	template <typename T>
	void StartTask(FAsyncTask<T>* AsyncTask)
	{
		FWriteScopeLock WriteLock(AsyncCreateLock);
		NumAsyncTaskStarted++;
		AsyncTask->StartBackgroundTask();
	}

	virtual void OnAsyncTaskExecutionComplete(FPointTask* AsyncTask, bool bSuccess);
	virtual bool IsAsyncWorkComplete();

	PCGExMT::FAsyncChunkedLoop MakeLoop() { return PCGExMT::FAsyncChunkedLoop(this, ChunkSize, bDoAsyncProcessing); }
};

class PCGEXTENDEDTOOLKIT_API FPCGExPointsProcessorElementBase : public FPCGPointProcessingElementBase
{
public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	virtual bool Validate(FPCGContext* InContext) const;
	virtual void InitializeContext(FPCGExPointsProcessorContext* InContext, const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) const;
	//virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPointTask : public FNonAbandonableTask
{
public:
	virtual ~FPointTask() = default;

	FPointTask(
		FPCGExPointsProcessorContext* InContext, UPCGExPointIO* InPointData, const PCGExMT::FTaskInfos& InInfos) :
		TaskContext(InContext), PointData(InPointData), Infos(InInfos)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncPointTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork() { if (IsTaskValid()) { ExecuteTask(); } }

	void ExecutionComplete(bool bSuccess)
	{
		if (!IsTaskValid()) { return; }
		TaskContext->OnAsyncTaskExecutionComplete(this, bSuccess);
	}

	void PostDoWork() { delete this; }

	virtual void ExecuteTask() = 0;

	FPCGExPointsProcessorContext* TaskContext;
	UPCGExPointIO* PointData;
	PCGExMT::FTaskInfos Infos;

protected:
	bool IsTaskValid() const
	{
		if (!TaskContext ||
			!TaskContext->SourceComponent.IsValid() ||
			TaskContext->SourceComponent.IsStale(true, true) ||
			TaskContext->NumAsyncTaskStarted == 0)
		{
			return false;
		}

		return true;
	}
};
