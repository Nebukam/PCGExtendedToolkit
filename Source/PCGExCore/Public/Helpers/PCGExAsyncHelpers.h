// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "Async/Async.h"

namespace PCGExAsyncHelpers
{
	struct FAsyncExecutionScope
	{
		FGraphEventArray Tasks;

		// No copy
		FAsyncExecutionScope(const FAsyncExecutionScope&) = delete;
		FAsyncExecutionScope& operator=(const FAsyncExecutionScope&) = delete;
    
		// No move (moving while tasks reference this could be dangerous)
		FAsyncExecutionScope(FAsyncExecutionScope&&) = delete;
		FAsyncExecutionScope& operator=(FAsyncExecutionScope&&) = delete;

		explicit FAsyncExecutionScope(int32 Reserve = 0) { Tasks.Reserve(Reserve); }
    
		~FAsyncExecutionScope()
		{
			if (Tasks.Num() > 0)
			{
				FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks);
			}
		}

		void Execute(TUniqueFunction<void()>&& Fn)
		{
			Tasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
				[Func = MoveTemp(Fn)]() mutable { Func(); },
				TStatId(),
				nullptr,
				ENamedThreads::AnyThread
			));
		}
	};
}
