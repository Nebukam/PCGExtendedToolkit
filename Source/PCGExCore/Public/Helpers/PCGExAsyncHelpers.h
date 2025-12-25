// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Tasks/Task.h"
#include "Async/Async.h"

namespace PCGExAsyncHelpers
{
	struct FBatchScope
	{
		FGraphEventArray Tasks;

		// No copy
		FBatchScope(const FBatchScope&) = delete;
		FBatchScope& operator=(const FBatchScope&) = delete;
    
		// No move (moving while tasks reference this could be dangerous)
		FBatchScope(FBatchScope&&) = delete;
		FBatchScope& operator=(FBatchScope&&) = delete;

		explicit FBatchScope(int32 Reserve = 0) { Tasks.Reserve(Reserve); }
    
		~FBatchScope()
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
