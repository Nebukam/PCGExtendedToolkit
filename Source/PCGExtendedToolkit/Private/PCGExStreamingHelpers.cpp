// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExStreamingHelpers.h"

#include "CoreMinimal.h"
#include "PCGExMT.h"
#include "UObject/Interface.h"
#include "GameFramework/Actor.h"
#include "Engine/AssetManager.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "UObject/SoftObjectPath.h"

namespace PCGExHelpers
{
	void LoadBlocking_AnyThread(const FSoftObjectPath& Path)
	{
		if (IsInGameThread())
		{
			// We're in the game thread, request synchronous load
			UAssetManager::GetStreamableManager().RequestSyncLoad(Path);
		}
		else
		{
			// We're not in the game thread, we need to dispatch loading to the main thread
			// and wait in the current one
			const FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([Path]()
			{
				const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestSyncLoad(Path);
			}, TStatId(), nullptr, ENamedThreads::GameThread);

			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
		}
	}

	void LoadBlocking_AnyThread(const TSharedPtr<TSet<FSoftObjectPath>>& Paths)
	{
		if (IsInGameThread())
		{
			UAssetManager::GetStreamableManager().RequestSyncLoad(Paths->Array());
		}
		else
		{
			TWeakPtr<TSet<FSoftObjectPath>> WeakPaths = Paths;
			const FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([WeakPaths]()
			{
				const TSharedPtr<TSet<FSoftObjectPath>> ToBeLoaded = WeakPaths.Pin();
				if (!ToBeLoaded) { return; }
				const TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestSyncLoad(ToBeLoaded->Array());
			}, TStatId(), nullptr, ENamedThreads::GameThread);

			FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
		}
	}

	void Load(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, FGetPaths&& GetPathsFunc, FOnLoadEnd&& OnLoadEnd)
	{
		check(AsyncManager);

		PCGExMT::ExecuteOnMainThread(AsyncManager, [GetPathsFunc, OnLoadEnd]()
		{
			TArray<FSoftObjectPath> Paths = GetPathsFunc();

			if (Paths.IsEmpty())
			{
				OnLoadEnd(false, nullptr);
				return;
			}

			const TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(Paths), [OnLoadEnd](TSharedPtr<FStreamableHandle> InHandle) // NOLINT(performance-unnecessary-value-param)
			{
				OnLoadEnd(true, InHandle);
			});

			if (!LoadHandle || !LoadHandle->IsActive())
			{
				if (!LoadHandle || !LoadHandle->HasLoadCompleted()) { OnLoadEnd(false, LoadHandle); }
				else { OnLoadEnd(true, LoadHandle); }
			}
		});
	}
}
