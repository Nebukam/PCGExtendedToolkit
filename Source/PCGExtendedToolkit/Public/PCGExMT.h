// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGContext.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGAsync.h"

namespace PCGExMT
{
	// Because I can't for the love of whomever add extern EVERYWHERE, it's admin hell
	// and apparently using __COUNTER__ was a very very very very bad idea
	// Testing FName at runtime is basically sabotage
	// So yeah, here you go, have a FName hash instead.
#define PCGEX_ASYNC_STATE(_NAME) const PCGExMT::AsyncState _NAME = GetTypeHash(FName(#_NAME));

	constexpr int32 GAsyncLoop_XS = 32;
	constexpr int32 GAsyncLoop_S = 64;
	constexpr int32 GAsyncLoop_M = 256;
	constexpr int32 GAsyncLoop_L = 512;
	constexpr int32 GAsyncLoop_XL = 1024;


	using AsyncState = uint64;

	PCGEX_ASYNC_STATE(State_Setup)
	PCGEX_ASYNC_STATE(State_ReadyForNextPoints)
	PCGEX_ASYNC_STATE(State_ProcessingPoints)

	PCGEX_ASYNC_STATE(State_ProcessingTargets)
	PCGEX_ASYNC_STATE(State_WaitingOnAsyncWork)
	PCGEX_ASYNC_STATE(State_WaitingOnAsyncProcessing)
	PCGEX_ASYNC_STATE(State_WaitingOnAsyncCompletion)
	PCGEX_ASYNC_STATE(State_Done)

	PCGEX_ASYNC_STATE(State_Processing)
	PCGEX_ASYNC_STATE(State_Completing)

	struct PCGEXTENDEDTOOLKIT_API FChunkedLoop
	{
		FChunkedLoop()
		{
		}

		int32 NumIterations = -1;
		int32 ChunkSize = 32;

		int32 CurrentIndex = -1;

		template <class InitializeFunc, class LoopBodyFunc>
		bool Advance(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody)
		{
			if (CurrentIndex == -1)
			{
				Initialize();
				CurrentIndex = 0;
			}
			return Advance(LoopBody);
		}

		template <class LoopBodyFunc>
		bool Advance(LoopBodyFunc&& LoopBody)
		{
			if (CurrentIndex == -1) { CurrentIndex = 0; }
			const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
			if (ChunkNumIterations > 0)
			{
				for (int i = 0; i < ChunkNumIterations; i++) { LoopBody(CurrentIndex + i); }
				CurrentIndex += ChunkNumIterations;
			}
			if (CurrentIndex >= NumIterations)
			{
				CurrentIndex = -1;
				return true;
			}
			return false;
		}

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FAsyncParallelLoop
	{
		FAsyncParallelLoop()
		{
		}

		FAsyncParallelLoop(FPCGContext* InContext, const int32 InChunkSize, const bool InEnabled):
			Context(InContext), ChunkSize(InChunkSize), bAsyncEnabled(InEnabled)
		{
		}

		FPCGContext* Context = nullptr;
		int32 ChunkSize = 32;
		bool bAsyncEnabled = true;
		int32 NumIterations = -1;

		int32 CurrentIndex = -1;

		template <class InitializeFunc, class LoopBodyFunc>
		bool Advance(InitializeFunc&& Initialize, LoopBodyFunc&& LoopBody)
		{
			if (bAsyncEnabled)
			{
				return FPCGAsync::AsyncProcessingOneToOneEx(
					&(Context->AsyncState), NumIterations, Initialize, [&](int32 ReadIndex, int32 WriteIndex)
					{
						LoopBody(ReadIndex);
						return true;
					}, true, ChunkSize);
			}

			if (CurrentIndex == -1)
			{
				Initialize();
				CurrentIndex = 0;
			}

			const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
			if (ChunkNumIterations <= 0)
			{
				CurrentIndex = -1;
				return true;
			}
			for (int i = 0; i < ChunkNumIterations; i++) { LoopBody(CurrentIndex + i); }
			CurrentIndex += ChunkNumIterations;
			return false;
		}

		template <class LoopBodyFunc>
		bool Advance(LoopBodyFunc&& LoopBody)
		{
			if (bAsyncEnabled)
			{
				return FPCGAsync::AsyncProcessingOneToOneEx(
					&(Context->AsyncState), NumIterations, []()
					{
					}, [&](int32 ReadIndex, int32 WriteIndex)
					{
						LoopBody(ReadIndex);
						return true;
					}, true, ChunkSize);
			}

			if (CurrentIndex == -1) { CurrentIndex = 0; }
			const int32 ChunkNumIterations = FMath::Min(NumIterations - CurrentIndex, GetCurrentChunkSize());
			if (ChunkNumIterations <= 0)
			{
				CurrentIndex = -1;
				return true;
			}
			for (int i = 0; i < ChunkNumIterations; i++) { LoopBody(CurrentIndex + i); }
			CurrentIndex += ChunkNumIterations;
			return false;
		}

	protected:
		int32 GetCurrentChunkSize() const
		{
			return FMath::Min(ChunkSize, NumIterations - CurrentIndex);
		}
	};

	/**
	 * 
	 * @tparam LoopBodyFunc 
	 * @param Context The context containing the AsyncState
	 * @param NumIterations The number of calls that will be done to the provided function, also an upper bound on the number of data generated.
	 * @param Initialize Called once when the processing starts
	 * @param LoopBody Signature: bool(int32 ReadIndex, int32 WriteIndex).
	 * @param ChunkSize Size of the chunks to cut the input data with
	 * @param bForceSync
	 * @return 
	 */
	static bool ParallelForLoop
		(
		FPCGContext* Context,
		const int32 NumIterations,
		TFunction<void()>&& Initialize,
		TFunction<void(int32)>&& LoopBody,
		const int32 ChunkSize = 32,
		const bool bForceSync = false)
	{
		if (bForceSync)
		{
			Initialize();
			for (int i = 0; i < NumIterations; i++) { LoopBody(i); }
			return true;
		}

		auto InnerBodyLoop = [&](const int32 ReadIndex, int32 WriteIndex)
		{
			LoopBody(ReadIndex);
			return true;
		};
		return FPCGAsync::AsyncProcessingOneToOneEx(&(Context->AsyncState), NumIterations, Initialize, InnerBodyLoop, true, ChunkSize);
	}

	static bool ParallelForLoop
		(
		FPCGContext* Context,
		const int32 NumIterations,
		TFunction<void(int32)>&& LoopBody,
		const int32 ChunkSize = 32,
		const bool bForceSync = false)
	{
		if (bForceSync)
		{
			for (int i = 0; i < NumIterations; i++) { LoopBody(i); }
			return true;
		}

		auto InnerBodyLoop = [&](const int32 ReadIndex, int32 WriteIndex)
		{
			LoopBody(ReadIndex);
			return true;
		};
		return FPCGAsync::AsyncProcessingOneToOneEx(
			&(Context->AsyncState), NumIterations, []()
			{
			}, InnerBodyLoop, true, ChunkSize);
	}
}

class FPCGExNonAbandonableTask;

class PCGEXTENDEDTOOLKIT_API FPCGExAsyncManager
{
	friend class FPCGExNonAbandonableTask;

public:
	~FPCGExAsyncManager();

	mutable FRWLock ManagerLock;
	FPCGContext* Context;
	bool bStopped = false;
	bool bForceSync = false;

	template <typename T, typename... Args>
	void Start(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
	{
		bool bDoSync = false;

		{
			FReadScopeLock ReadLock(ManagerLock);
			if (bStopped || bFlushing) { return; }
			bDoSync = bForceSync;
		}

		if (bDoSync) { StartSynchronousTask<T>(new FAsyncTask<T>(InPointsIO, args...), TaskIndex); }
		else { StartBackgroundTask<T>(new FAsyncTask<T>(InPointsIO, args...), TaskIndex); }
	}

	template <typename T, typename... Args>
	void StartSynchronous(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
	{
		{
			FReadScopeLock ReadLock(ManagerLock);
			if (bStopped || bFlushing) { return; }
		}

		StartSynchronousTask(new FAsyncTask<T>(InPointsIO, args...), TaskIndex);
	}

	template <typename T>
	void StartBackgroundTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
	{
		{
			FWriteScopeLock WriteLock(ManagerLock);
			if (bStopped || bFlushing) { return; }
			NumStarted++;
			QueuedTasks.Add(AsyncTask);
		}

		T& Task = AsyncTask->GetTask();
		Task.TaskPtr = AsyncTask;
		Task.Manager = this;
		Task.TaskIndex = TaskIndex;

		AsyncTask->StartBackgroundTask();
	}

	template <typename T>
	void StartSynchronousTask(FAsyncTask<T>* AsyncTask, int32 TaskIndex = -1)
	{
		{
			FReadScopeLock ReadScopeLock(ManagerLock);
			if (bStopped || bFlushing) { return; }
		}

		T& Task = AsyncTask->GetTask();
		Task.TaskPtr = AsyncTask;
		Task.Manager = this;
		Task.TaskIndex = TaskIndex;
		Task.bIsAsync = false;

		Task.ExecuteTask();
		delete AsyncTask;
	}


	void Reserve(const int32 NumTasks) { QueuedTasks.Reserve(NumTasks); }

	void OnAsyncTaskExecutionComplete(FPCGExNonAbandonableTask* AsyncTask, bool bSuccess);
	bool IsAsyncWorkComplete() const;

	void Reset();

	template <typename T>
	T* GetContext() { return static_cast<T*>(Context); }

protected:
	bool bFlushing = false;
	int32 NumStarted = 0;
	int32 NumCompleted = 0;
	TSet<FAsyncTaskBase*> QueuedTasks;
};

class PCGEXTENDEDTOOLKIT_API FPCGExNonAbandonableTask : public FNonAbandonableTask
{
	friend class FPCGExAsyncManager;

protected:
	bool bIsAsync = true;

public:
	virtual ~FPCGExNonAbandonableTask() = default;
	FPCGExAsyncManager* Manager = nullptr;
	int32 TaskIndex = -1;
	FAsyncTaskBase* TaskPtr = nullptr;
	PCGExData::FPointIO* PointIO = nullptr;

#define PCGEX_ASYNC_CHECKPOINT_VOID  if (!Checkpoint()) { return; }
#define PCGEX_ASYNC_CHECKPOINT  if (!Checkpoint()) { return false; }

	FPCGExNonAbandonableTask(PCGExData::FPointIO* InPointIO) : PointIO(InPointIO)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPCGExAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork()
	{
		if (bWorkDone) { return; }
		PCGEX_ASYNC_CHECKPOINT_VOID
		bWorkDone = true;
		Manager->OnAsyncTaskExecutionComplete(this, ExecuteTask());
	}

	virtual bool ExecuteTask() = 0;

protected:
	bool bWorkDone = false;
	bool Checkpoint() const { return !(!Manager || Manager->bStopped || Manager->bFlushing); }

	template <typename T, typename... Args>
	void InternalStart(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
	{
		PCGEX_ASYNC_CHECKPOINT_VOID
		if (!bIsAsync) { Manager->StartSynchronous<T>(TaskIndex, InPointsIO, args...); }
		else { Manager->Start<T>(TaskIndex, InPointsIO, args...); }
	}
};

class PCGEXTENDEDTOOLKIT_API FPCGExLoopChunkTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExLoopChunkTask(PCGExData::FPointIO* InPointIO,
	                    const int32 InNumIterations) :
		FPCGExNonAbandonableTask(InPointIO),
		NumIterations(InNumIterations)
	{
	}

	int32 NumIterations = 0;

	virtual bool ExecuteTask() override
	{
		for (int i = 0; i < NumIterations; i++) { LoopBody(TaskIndex + i); }
		return true;
	}

	virtual void LoopBody(const int32 Iteration) = 0;
};

template <typename T>
class PCGEXTENDEDTOOLKIT_API FPCGExParallelLoopTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExParallelLoopTask(PCGExData::FPointIO* InPointIO,
	                       const int32 InNumIterations,
	                       const int32 InChunkSize) :
		FPCGExNonAbandonableTask(InPointIO),
		NumIterations(InNumIterations),
		ChunkSize(InChunkSize)
	{
	}

	int32 NumIterations = 0;
	int32 ChunkSize = 0;

	virtual bool ExecuteTask() override
	{
		if (!LoopInit()) { return false; }
		const int32 LoopCount = FMath::Max(NumIterations / ChunkSize, 1);
		int32 StartIndex = 0;

		for (int i = 0; i < LoopCount; i++)
		{
			const int32 LoopNumIterations = FMath::Min(NumIterations, FMath::Min(ChunkSize, NumIterations - (ChunkSize * i)));
			if (LoopNumIterations == 0) { break; }
			InternalStart<T>(StartIndex, PointIO, LoopNumIterations);
			StartIndex += ChunkSize;
		}

		return true;
	}

	virtual bool LoopInit() { return true; }
};
