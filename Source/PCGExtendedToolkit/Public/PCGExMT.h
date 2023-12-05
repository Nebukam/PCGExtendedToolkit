// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGContext.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGMetadataCommon.h"

#include "PCGExMT.generated.h"

namespace PCGExMT
{
	using AsyncState = int64;

	constexpr AsyncState State_Setup = TNumericLimits<int64>::Min();
	constexpr AsyncState State_ReadyForNextPoints = 1;
	constexpr AsyncState State_ProcessingPoints = 2;
	constexpr AsyncState State_WaitingOnAsyncWork = 3;
	constexpr AsyncState State_Done = TNumericLimits<int64>::Max();

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
			const int32 ChunkNumIterations = GetCurrentChunkSize();
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

	struct PCGEXTENDEDTOOLKIT_API FAsyncChunkedLoop
	{
		FAsyncChunkedLoop()
		{
		}

		FAsyncChunkedLoop(FPCGContext* InContext, int32 InChunkSize, bool InEnabled):
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
			const int32 ChunkNumIterations = GetCurrentChunkSize();
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
			const int32 ChunkNumIterations = GetCurrentChunkSize();
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

	struct PCGEXTENDEDTOOLKIT_API FTaskInfos
	{
		FTaskInfos()
		{
		}

		FTaskInfos(int32 InIndex, PCGMetadataEntryKey InKey):
			Index(InIndex), Key(InKey)
		{
		}

		FTaskInfos(const FTaskInfos& Other):
			Index(Other.Index), Key(Other.Key)
		{
		}

		int32 Index = -1;
		PCGMetadataEntryKey Key = PCGInvalidEntryKey;
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
		bool bForceSync = false)
	{
		if (bForceSync)
		{
			Initialize();
			for (int i = 0; i < NumIterations; i++) { LoopBody(i); }
			return true;
		}

		auto InnerBodyLoop = [&](int32 ReadIndex, int32 WriteIndex)
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
		bool bForceSync = false)
	{
		if (bForceSync)
		{
			for (int i = 0; i < NumIterations; i++) { LoopBody(i); }
			return true;
		}

		auto InnerBodyLoop = [&](int32 ReadIndex, int32 WriteIndex)
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

UCLASS(Blueprintable, EditInlineNew)
class PCGEXTENDEDTOOLKIT_API UPCGExAsyncTaskManager : public UObject
{
	GENERATED_BODY()

	friend class FPCGExAsyncTask;

public:
	mutable FRWLock ManagerLock;
	FPCGContext* Context;

	template <typename T, typename... Args>
	void StartTask(int32 Index, PCGMetadataAttributeKey Key, UPCGExPointIO* InPointsIO, Args... args)
	{
		{
			FWriteScopeLock WriteLock(ManagerLock);
			NumStarted++;
		}
		FAutoDeleteAsyncTask<T>* AsyncTask = new FAutoDeleteAsyncTask<T>(this, PCGExMT::FTaskInfos(Index, Key), InPointsIO, args...);
		AsyncTask->StartBackgroundTask();
	}

	void OnAsyncTaskExecutionComplete(FPCGExAsyncTask* AsyncTask, bool bSuccess);
	bool IsAsyncWorkComplete() const;

	void Reset()
	{
		NumStarted = 0;
		NumCompleted = 0;
	}

	template <typename T>
	T* GetContext() { return static_cast<T*>(Context); }

protected:
	int32 NumStarted = 0;
	int32 NumCompleted = 0;

	bool IsValid() const;
};

class PCGEXTENDEDTOOLKIT_API FPCGExAsyncTask : public FNonAbandonableTask
{
public:
	virtual ~FPCGExAsyncTask() = default;

	FPCGExAsyncTask(
		UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos, UPCGExPointIO* InPointIO) :
		Manager(InManager), TaskInfos(InInfos), PointIO(InPointIO)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncPointTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	void DoWork()
	{
		Manager->OnAsyncTaskExecutionComplete(this, ExecuteTask());
	}

	virtual bool ExecuteTask() = 0;

	UPCGExAsyncTaskManager* Manager = nullptr;
	PCGExMT::FTaskInfos TaskInfos;
	UPCGExPointIO* PointIO = nullptr;
	bool bDropped = false;

protected:
	bool CanContinue() const
	{
		if (!Manager || !Manager->IsValid()) { return false; }
		return true;
	}
};
