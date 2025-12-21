// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExStreamingHelpers.h"

#include "CoreMinimal.h"
#include "Core/PCGExContext.h"
#include "Engine/AssetManager.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/StreamableManager.h"

namespace PCGExHelpers
{
	TSharedPtr<FStreamableHandle> LoadBlocking_AnyThread(const FSoftObjectPath& Path, FPCGExContext* InContext)
	{
		TSharedPtr<FStreamableHandle> Handle;
		if (IsInGameThread())
		{
			// We're in the game thread, request synchronous load
			Handle = UAssetManager::GetStreamableManager().RequestSyncLoad(Path);
			if (InContext) { InContext->TrackAssetsHandle(Handle); }
		}
		else
		{
			PCGExMT::ExecuteOnMainThreadAndWait([&]() { Handle = LoadBlocking_AnyThread(Path, InContext); });
		}

		return Handle;
	}

	TSharedPtr<FStreamableHandle> LoadBlocking_AnyThread(const TSharedPtr<TSet<FSoftObjectPath>>& Paths, FPCGExContext* InContext)
	{
		TSharedPtr<FStreamableHandle> Handle;
		if (IsInGameThread())
		{
			Handle = UAssetManager::GetStreamableManager().RequestSyncLoad(Paths->Array());
			if (InContext) { InContext->TrackAssetsHandle(Handle); }
		}
		else
		{
			PCGExMT::ExecuteOnMainThreadAndWait([&]() { Handle = LoadBlocking_AnyThread(Paths, InContext); });
		}
		return Handle;
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

	void SafeReleaseHandle(TSharedPtr<FStreamableHandle>& InHandle)
	{
		if (!InHandle.IsValid()) { return; }

		if (IsInGameThread())
		{
			InHandle->ReleaseHandle();
			InHandle.Reset();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [Handle = MoveTemp(InHandle)]()
			{
				if (Handle.IsValid()) { Handle->ReleaseHandle(); }
			});
		}
	}

	void SafeReleaseHandles(TArray<TSharedPtr<FStreamableHandle>>& InHandles)
	{
		if (InHandles.IsEmpty()) { return; }

		if (IsInGameThread())
		{
			for (TSharedPtr<FStreamableHandle>& Handle : InHandles)
			{
				if (Handle.IsValid()) { Handle->ReleaseHandle(); }
			}
			InHandles.Empty();
		}
		else
		{
			AsyncTask(ENamedThreads::GameThread, [Handles = MoveTemp(InHandles)]()
			{
				for (const TSharedPtr<FStreamableHandle>& Handle : Handles)
				{
					if (Handle.IsValid()) { Handle->ReleaseHandle(); }
				}
			});
		}
	}
}
