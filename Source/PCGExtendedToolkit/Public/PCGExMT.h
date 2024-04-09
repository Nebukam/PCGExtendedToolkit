// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGContext.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGAsync.h"

namespace PCGExMT
{
	constexpr int32 GAsyncLoop_XS = 32;
	constexpr int32 GAsyncLoop_S = 64;
	constexpr int32 GAsyncLoop_M = 256;
	constexpr int32 GAsyncLoop_L = 512;
	constexpr int32 GAsyncLoop_XL = 1024;


	using AsyncState = int64;

	constexpr AsyncState State_Setup = __COUNTER__;
	constexpr AsyncState State_ReadyForNextPoints = __COUNTER__;
	constexpr AsyncState State_ProcessingPoints = __COUNTER__;
	constexpr AsyncState State_WaitingOnAsyncWork = __COUNTER__;
	constexpr AsyncState State_Done = __COUNTER__;

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

	template <typename T>
	void Start(FAsyncTask<T>* AsyncTask)
	{
		{
			FWriteScopeLock WriteLock(ManagerLock);
			if (bStopped) { return; }
			NumStarted++;
			QueuedTasks.Add(AsyncTask);
		}

		T& Task = AsyncTask->GetTask();
		Task.TaskPtr = AsyncTask;

		AsyncTask->StartBackgroundTask();
	}

	template <typename T, typename... Args>
	void Start(int32 TaskIndex, PCGExData::FPointIO* InPointsIO, Args... args)
	{
		if (bStopped) { return; }
		if (bForceSync) { StartSync<T>(new FAsyncTask<T>(this, TaskIndex, InPointsIO, args...)); }
		else { Start<T>(new FAsyncTask<T>(this, TaskIndex, InPointsIO, args...)); }
	}

	template <typename T>
	void StartSync(FAsyncTask<T>* AsyncTask)
	{
		{
			FWriteScopeLock WriteLock(ManagerLock);
			if (bStopped) { return; }
			NumStarted++;
		}

		T& Task = AsyncTask->GetTask();
		Task.TaskPtr = AsyncTask;

		AsyncTask->StartSynchronousTask();
	}

	template <typename T, typename... Args>
	void StartSync(int32 Index, PCGExData::FPointIO* InPointsIO, Args... args)
	{
		if (bStopped) { return; }
		StartSync(new FAsyncTask<T>(this, Index, InPointsIO, args...));
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
public:
	FPCGExAsyncManager* Manager = nullptr;
	int32 TaskIndex = -1;
	FAsyncTaskBase* TaskPtr = nullptr;
	PCGExData::FPointIO* PointIO = nullptr;

#define PCGEX_ASYNC_CHECKPOINT_VOID  if (!Checkpoint()) { return; }
#define PCGEX_ASYNC_CHECKPOINT  if (!Checkpoint()) { return false; }

	FPCGExNonAbandonableTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		Manager(InManager), TaskIndex(InTaskIndex), PointIO(InPointIO)
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
};


template <class TBodyFunc>
class PCGEXTENDEDTOOLKIT_API FPCGExLoopChunkTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExLoopChunkTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		TBodyFunc&& InBodyFunc,
		const int32 InNumIterations) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		BodyFunc(InBodyFunc),
		NumIterations(InNumIterations)
	{
	}

	TBodyFunc&& BodyFunc;
	int32 NumIterations = 0;

	virtual bool ExecuteTask() override
	{
		for (int i = 0; i < NumIterations; i++) { BodyFunc(TaskIndex + i); }
		return true;
	}
};

template <class TInitFunc, class TBodyFunc>
class PCGEXTENDEDTOOLKIT_API FPCGExParallelLoopTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExParallelLoopTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		TInitFunc&& InInitFunc,
		TBodyFunc&& InBodyFunc,
		const int32 InNumIterations,
		const int32 InChunkSize) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		InitFunc(InInitFunc),
		BodyFunc(InBodyFunc),
		NumIterations(InNumIterations),
		ChunkSize(InChunkSize)
	{
	}

	TInitFunc&& InitFunc;
	TBodyFunc&& BodyFunc;
	int32 NumIterations = 0;
	int32 ChunkSize = 0;

	virtual bool ExecuteTask() override
	{
		InitFunc();

		const int32 LoopCount = FMath::Max(NumIterations / ChunkSize, 1);
		int32 StartIndex = 0;

		for (int i = 0; i < LoopCount; i++)
		{
			const int32 LoopNumIterations = FMath::Min(NumIterations, FMath::Min(ChunkSize, NumIterations - (ChunkSize * i)));
			if (LoopNumIterations == 0) { break; }
			Manager->Start<FPCGExLoopChunkTask<TBodyFunc>>(StartIndex, nullptr, BodyFunc, LoopNumIterations);
			StartIndex += ChunkSize;
		}

		return true;
	}
};
