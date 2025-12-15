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
			PCGExMT::ExecuteOnMainThreadAndWait([&]() { LoadBlocking_AnyThread(Path); });
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
			PCGExMT::ExecuteOnMainThreadAndWait([&]() { LoadBlocking_AnyThread(Paths); });
		}
	}

	void Load(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, FGetPaths&& GetPathsFunc, FOnLoadEnd&& OnLoadEnd)
	{
		check(TaskManager);

		PCGExMT::ExecuteOnMainThread(TaskManager, [GetPathsFunc, OnLoadEnd, TaskManager]()
		{
			TArray<FSoftObjectPath> Paths = GetPathsFunc();

			if (Paths.IsEmpty())
			{
				OnLoadEnd(false, nullptr);
				return;
			}

			TWeakPtr<PCGExMT::FAsyncToken> LoadToken = TaskManager->TryCreateToken(FName("LoadToken"));
			const TSharedPtr<FStreamableHandle> LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				MoveTemp(Paths),
				[OnLoadEnd, LoadToken](TSharedPtr<FStreamableHandle> InHandle) // NOLINT(performance-unnecessary-value-param)
				{
					OnLoadEnd(true, InHandle);
					PCGEX_ASYNC_RELEASE_CAPTURED_TOKEN(LoadToken)
				});

			if (!LoadHandle || !LoadHandle->IsActive())
			{
				if (!LoadHandle || !LoadHandle->HasLoadCompleted()) { OnLoadEnd(false, LoadHandle); }
				else { OnLoadEnd(true, LoadHandle); }
				PCGEX_ASYNC_RELEASE_TOKEN(LoadToken)
			}
		});
	}
}
