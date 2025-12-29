// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/



#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"

#include "Async/Async.h"

struct FPCGExContext;
struct FStreamableHandle;

namespace PCGExMT
{
	class IAsyncHandleGroup;
	class FTaskManager;
}

namespace PCGExHelpers
{
	using FGetPaths = std::function<TArray<FSoftObjectPath>()>;
	using FOnLoadEnd = std::function<void(const bool bSuccess, TSharedPtr<FStreamableHandle> StreamableHandle)>;

	PCGEXCORE_API
	TSharedPtr<FStreamableHandle> LoadBlocking_AnyThread(const FSoftObjectPath& Path, FPCGExContext* InContext = nullptr);

	template <typename T>
	static TSharedPtr<FStreamableHandle> LoadBlocking_AnyThreadTpl(const TSoftObjectPtr<T>& SoftObjectPtr, FPCGExContext* InContext = nullptr)
	{
		return LoadBlocking_AnyThread(SoftObjectPtr.ToSoftObjectPath(), InContext);
	}

	PCGEXCORE_API
	TSharedPtr<FStreamableHandle> LoadBlocking_AnyThread(const TSharedPtr<TSet<FSoftObjectPath>>& Paths, FPCGExContext* InContext = nullptr);

	PCGEXCORE_API
	void Load(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, FGetPaths&& GetPathsFunc, FOnLoadEnd&& OnLoadEnd);

	PCGEXCORE_API
	void SafeReleaseHandle(TSharedPtr<FStreamableHandle>& InHandle);

	PCGEXCORE_API
	void SafeReleaseHandles(TArray<TSharedPtr<FStreamableHandle>>& InHandles);
}
