// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGExAsync.h"
#include "Helpers/PCGAsync.h"

#include "PCGContext.h"
#include "PCGPoint.h"

#include "Async/Async.h"

namespace FPCGExAsync
{
	TAutoConsoleVariable<bool> ConsoleVar::CVarDisableAsyncTimeSlicing {
		TEXT("pcg.DisableAsyncTimeSlicing"),
		false,
		TEXT("To help debug, we can disable time slicing for async tasks.")
	};

	TAutoConsoleVariable<int32> ConsoleVar::CVarAsyncOverrideChunkSize{
		TEXT("pcg.AsyncOverrideChunkSize"),
		-1,
		TEXT("For quick benchmarking, we can override the value of chunk size for async processing. Any negative value is discarded.")
	};

	
	void AsyncPointProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<int32(int32, int32)>& IterationInnerLoop)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing);

		check(NumAvailableTasks > 0 && MinIterationsPerTask > 0 && NumIterations >= 0);
		if (NumAvailableTasks <= 0 || MinIterationsPerTask <= 0 || NumIterations <= 0)
		{
			return;
		}

		// Get number of available threads from the context
		const int32 NumTasks = FMath::Min(NumAvailableTasks, FMath::Max(1, NumIterations / MinIterationsPerTask));
		const int32 IterationsPerTask = NumIterations / NumTasks;
		const int32 NumFutures = NumTasks - 1;
		 
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing::AllocatingArray);
			// Pre-reserve the out points array
			OutPoints.SetNumUninitialized(NumIterations);
		}

		// Setup [current, last, nb points] data per dispatch
		TArray<TFuture<int32>> AsyncTasks;
		AsyncTasks.Reserve(NumFutures);

		// Launch the async tasks
		for (int32 TaskIndex = 0; TaskIndex < NumFutures; ++TaskIndex)
		{
			const int32 StartIndex = TaskIndex * IterationsPerTask;
			const int32 EndIndex = StartIndex + IterationsPerTask;

			AsyncTasks.Emplace(Async(EAsyncExecution::ThreadPool, [&IterationInnerLoop, StartIndex, EndIndex]()
			{
				return IterationInnerLoop(StartIndex, EndIndex);
			}));
		}

		// Execute last batch locally
		int32 NumPointsWrittenOnThisThread = IterationInnerLoop(NumFutures * IterationsPerTask, NumIterations);

		// Wait/Gather results & collapse points
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing::WaitAndCollapseArray);

			auto CollapsePoints = [&OutPoints](int32 RangeIndex, int32 StartPointsIndex, int32 NumPointsToCollapse)
			{
				// Move points from [StartsPointIndex, StartsPointIndex + NumberPointsAdded] to [RangeIndex, RangeIndex + NumberPointsAdded]
				if (StartPointsIndex != RangeIndex)
				{
					for (int32 MoveIndex = 0; MoveIndex < NumPointsToCollapse; ++MoveIndex)
					{
						OutPoints[RangeIndex + MoveIndex] = MoveTemp(OutPoints[StartPointsIndex + MoveIndex]);
					}
				}

				return RangeIndex + NumPointsToCollapse;
			};

			int RangeIndex = 0;
			for (int32 AsyncIndex = 0; AsyncIndex < AsyncTasks.Num(); ++AsyncIndex)
			{
				const int32 StartPointsIndex = AsyncIndex * IterationsPerTask;

				TFuture<int32>& AsyncTask = AsyncTasks[AsyncIndex];
				AsyncTask.Wait();
				const int32 NumberOfPointsAdded = AsyncTask.Get();
				RangeIndex = CollapsePoints(RangeIndex, StartPointsIndex, NumberOfPointsAdded);
			}

			// Finally, add current thread points
			RangeIndex = CollapsePoints(RangeIndex, NumFutures * IterationsPerTask, NumPointsWrittenOnThisThread);

			OutPoints.SetNum(RangeIndex);
		}
	}

	void AsyncPointProcessingEx(FPCGContext* Context, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<bool(int32, FPCGPoint&)>& PointFunc, const int32 MinIterationsPerTask )
	{
		
		FPCGAsyncState* AsyncState = Context ? &Context->AsyncState : nullptr;
		
		if (AsyncState && !AsyncState->bIsRunningAsyncCall)
		{
			AsyncState->bIsRunningAsyncCall = true;
			AsyncPointProcessing(AsyncState->NumAvailableTasks, MinIterationsPerTask, NumIterations, OutPoints, PointFunc);
			AsyncState->bIsRunningAsyncCall = false;
		}
		else
		{
			AsyncPointProcessing(1, MinIterationsPerTask, NumIterations, OutPoints, PointFunc);
		}
	}

	void AsyncPointProcessingEx(FPCGContext* Context, const TArray<FPCGPoint>& InPoints, TArray<FPCGPoint>& OutPoints, const TFunction<bool(const FPCGPoint&, FPCGPoint&)>& PointFunc, const int32 MinIterationsPerTask)
	{

		auto IterationInnerLoop = [&PointFunc, &InPoints, &OutPoints](int32 StartIndex, int32 EndIndex) -> int32
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing::InnerLoop);
			int32 NumPointsWritten = 0;

			for (int32 Index = StartIndex; Index < EndIndex; ++Index)
			{
				if (PointFunc(InPoints[Index], OutPoints[StartIndex + NumPointsWritten]))
				{
					++NumPointsWritten;
				}
			}

			return NumPointsWritten;
		};

		FPCGAsyncState* AsyncState = Context ? &Context->AsyncState : nullptr;

		if (AsyncState && !AsyncState->bIsRunningAsyncCall)
		{
			AsyncState->bIsRunningAsyncCall = true;
			AsyncPointProcessing(AsyncState->NumAvailableTasks, MinIterationsPerTask, InPoints.Num(), OutPoints, IterationInnerLoop);
			AsyncState->bIsRunningAsyncCall = false;
		}
		else
		{
			AsyncPointProcessing(1, MinIterationsPerTask, InPoints.Num(), OutPoints, IterationInnerLoop);
		}
	}

	void AsyncPointProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<bool(int32, FPCGPoint&)>& PointFunc)
	{
		auto IterationInnerLoop = [&PointFunc, &OutPoints](int32 StartIndex, int32 EndIndex) -> int32
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing::InnerLoop);
			int32 NumPointsWritten = 0;

			for (int32 Index = StartIndex; Index < EndIndex; ++Index)
			{
				if (PointFunc(Index, OutPoints[StartIndex + NumPointsWritten]))
				{
					++NumPointsWritten;
				}
			}

			return NumPointsWritten;
		};

		AsyncPointProcessing(NumAvailableTasks, MinIterationsPerTask, NumIterations, OutPoints, IterationInnerLoop);
	}

	void AsyncPointFilterProcessingEx(FPCGContext* Context, int32 NumIterations, TArray<FPCGPoint>& InFilterPoints, TArray<FPCGPoint>& OutFilterPoints, const TFunction<bool(int32, FPCGPoint&, FPCGPoint&)>& PointFunc, const int32 MinIterationsPerTask)
	{

		FPCGAsyncState* AsyncState = Context ? &Context->AsyncState : nullptr;

		if (AsyncState && !AsyncState->bIsRunningAsyncCall)
		{
			AsyncState->bIsRunningAsyncCall = true;
			AsyncPointFilterProcessing(AsyncState->NumAvailableTasks, MinIterationsPerTask, NumIterations, InFilterPoints, OutFilterPoints, PointFunc);
			AsyncState->bIsRunningAsyncCall = false;
		}
		else
		{
			AsyncPointFilterProcessing(1, MinIterationsPerTask, NumIterations, InFilterPoints, OutFilterPoints, PointFunc);
		}
	}

	void AsyncPointFilterProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& InFilterPoints, TArray<FPCGPoint>& OutFilterPoints, const TFunction<bool(int32, FPCGPoint&, FPCGPoint&)>& PointFunc)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointFilterProcessing);
		check(NumAvailableTasks > 0 && MinIterationsPerTask > 0 && NumIterations >= 0);
		if (NumAvailableTasks <= 0 || MinIterationsPerTask <= 0 || NumIterations <= 0)
		{
			return;
		}

		// Get number of available threads from the context
		const int32 NumTasks = FMath::Min(NumAvailableTasks, FMath::Max(1, NumIterations / MinIterationsPerTask));
		const int32 IterationsPerTask = NumIterations / NumTasks;
		const int32 NumFutures = NumTasks - 1;

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing::AllocatingArray);
			// Pre-reserve the out points array
			InFilterPoints.SetNumUninitialized(NumIterations);
			OutFilterPoints.SetNumUninitialized(NumIterations);
		}

		auto IterationInnerLoop = [&PointFunc, &InFilterPoints, &OutFilterPoints](int32 StartIndex, int32 EndIndex) -> TPair<int32, int32>
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointProcessing::InnerLoop);
			int32 NumPointsInWritten = 0;
			int32 NumPointsOutWritten = 0;

			for (int32 Index = StartIndex; Index < EndIndex; ++Index)
			{
				if (PointFunc(Index, InFilterPoints[StartIndex + NumPointsInWritten], OutFilterPoints[StartIndex + NumPointsOutWritten]))
				{
					++NumPointsInWritten;
				}
				else
				{
					++NumPointsOutWritten;
				}
			}

			return TPair<int32, int32>(NumPointsInWritten, NumPointsOutWritten);
		};

		// Setup [current, last, nb points] data per dispatch
		TArray<TFuture<TPair<int32, int32>>> AsyncTasks;
		AsyncTasks.Reserve(NumFutures);

		// Launch the async tasks
		for (int32 TaskIndex = 0; TaskIndex < NumFutures; ++TaskIndex)
		{
			const int32 StartIndex = TaskIndex * IterationsPerTask;
			const int32 EndIndex = StartIndex + IterationsPerTask;

			AsyncTasks.Emplace(Async(EAsyncExecution::ThreadPool, [&IterationInnerLoop, StartIndex, EndIndex]() -> TPair<int32, int32>
			{
				return IterationInnerLoop(StartIndex, EndIndex);
			}));
		}

		// Launch remainder on current thread
		TPair<int32, int32> NumPointsWrittenOnThisThread = IterationInnerLoop(NumFutures * IterationsPerTask, NumIterations);

		// Wait/Gather results & collapse points
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncPointFilterProcessing::WaitAndCollapseArray);

			auto CollapsePoints = [&InFilterPoints, &OutFilterPoints](const TPair<int32, int32>& RangeIndices, int32 StartPointsIndex, const TPair<int32, int32>& NumberOfPointsAdded) -> TPair<int32, int32>
			{
				int32 InFilterRangeIndex = RangeIndices.Key;
				int32 OutFilterRangeIndex = RangeIndices.Value;

				// Move in-filter points
				{
					int NumInFilterPoints = NumberOfPointsAdded.Key;

					if (StartPointsIndex != InFilterRangeIndex)
					{
						for (int32 MoveIndex = 0; MoveIndex < NumInFilterPoints; ++MoveIndex)
						{
							InFilterPoints[InFilterRangeIndex + MoveIndex] = MoveTemp(InFilterPoints[StartPointsIndex + MoveIndex]);
						}
					}

					InFilterRangeIndex += NumInFilterPoints;
				}

				// Move out-filter points
				{
					int NumOutFilterPoints = NumberOfPointsAdded.Value;

					if (StartPointsIndex != OutFilterRangeIndex)
					{
						for (int32 MoveIndex = 0; MoveIndex < NumOutFilterPoints; ++MoveIndex)
						{
							OutFilterPoints[OutFilterRangeIndex + MoveIndex] = MoveTemp(OutFilterPoints[StartPointsIndex + MoveIndex]);
						}
					}

					OutFilterRangeIndex += NumOutFilterPoints;
				}

				return TPair<int32, int32>(InFilterRangeIndex, OutFilterRangeIndex);
			};

			TPair<int32, int32> RangeIndices = TPair<int32, int32>(0, 0);
			for (int32 AsyncIndex = 0; AsyncIndex < AsyncTasks.Num(); ++AsyncIndex)
			{
				const int32 StartPointsIndex = AsyncIndex * IterationsPerTask;

				TFuture<TPair<int32, int32>>& AsyncTask = AsyncTasks[AsyncIndex];
				AsyncTask.Wait();
				TPair<int32, int32> NumberOfPointsAdded = AsyncTask.Get();

				RangeIndices = CollapsePoints(RangeIndices, StartPointsIndex, NumberOfPointsAdded);
			}

			// Finally, add this thread results
			RangeIndices = CollapsePoints(RangeIndices, NumFutures * IterationsPerTask, NumPointsWrittenOnThisThread);

			InFilterPoints.SetNum(RangeIndices.Key);
			OutFilterPoints.SetNum(RangeIndices.Value);
		}
	}

	void AsyncMultiPointProcessingEx(FPCGContext* Context, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<TArray<FPCGPoint>(int32)>& PointFunc, const int32 MinIterationsPerTask)
	{
		FPCGAsyncState* AsyncState = Context ? &Context->AsyncState : nullptr;

		if (AsyncState && !AsyncState->bIsRunningAsyncCall)
		{
			AsyncState->bIsRunningAsyncCall = true;
			AsyncMultiPointProcessing(AsyncState->NumAvailableTasks, MinIterationsPerTask, NumIterations, OutPoints, PointFunc);
			AsyncState->bIsRunningAsyncCall = false;
		}
		else
		{
			AsyncMultiPointProcessing(1, MinIterationsPerTask, NumIterations, OutPoints, PointFunc);
		}
	}

	void AsyncMultiPointProcessing(int32 NumAvailableTasks, int32 MinIterationsPerTask, int32 NumIterations, TArray<FPCGPoint>& OutPoints, const TFunction<TArray<FPCGPoint>(int32)>& PointFunc)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncMultiPointProcessing);
		check(NumAvailableTasks > 0 && MinIterationsPerTask > 0 && NumIterations >= 0);
		if (NumAvailableTasks <= 0 || MinIterationsPerTask <= 0 || NumIterations <= 0)
		{
			return;
		}

		// Get number of available threads from the context
		const int32 NumTasks = FMath::Min(NumAvailableTasks, FMath::Max(1, NumIterations / MinIterationsPerTask));
		const int32 IterationsPerTask = NumIterations / NumTasks;
		const int32 NumFutures = NumTasks - 1;

		auto IterationInnerLoop = [&PointFunc](int32 StartIndex, int32 EndIndex) -> TArray<FPCGPoint>
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncMultiPointProcessing::InnerLoop);
			TArray<FPCGPoint> OutPoints;

			for (int32 Index = StartIndex; Index < EndIndex; ++Index)
			{
				OutPoints.Append(PointFunc(Index));
			}

			return OutPoints;
		};

		TArray<TFuture<TArray<FPCGPoint>>> AsyncTasks;
		AsyncTasks.Reserve(NumFutures);

		// Launch the async tasks
		for (int32 TaskIndex = 0; TaskIndex < NumFutures; ++TaskIndex)
		{
			const int32 StartIndex = TaskIndex * IterationsPerTask;
			const int32 EndIndex = StartIndex + IterationsPerTask;

			AsyncTasks.Emplace(Async(EAsyncExecution::ThreadPool, [&IterationInnerLoop, StartIndex, EndIndex]() -> TArray<FPCGPoint>
			{
				return IterationInnerLoop(StartIndex, EndIndex);
			}));
		}

		TArray<FPCGPoint> PointsFromThisThread = IterationInnerLoop(NumFutures * IterationsPerTask, NumIterations);

		// Wait/Gather results & collapse points
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IPCGElement::AsyncMultiPointProcessing::WaitAndCollapseArray);
			for (TFuture<TArray<FPCGPoint>>& AsyncTask : AsyncTasks)
			{
				AsyncTask.Wait();
				OutPoints.Append(AsyncTask.Get());
			}

			OutPoints.Append(PointsFromThisThread);
		}
	}
}