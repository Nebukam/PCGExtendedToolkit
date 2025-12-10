// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Most helpers here are Copyright Epic Games, Inc. All Rights Reserved, cherry picked for 5.3

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/SoftObjectPath.h"

#include "Async/Async.h"

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

	PCGEXTENDEDTOOLKIT_API void LoadBlocking_AnyThread(const FSoftObjectPath& Path);

	template <typename T>
	static TObjectPtr<T> LoadBlocking_AnyThread(const TSoftObjectPtr<T>& SoftObjectPtr, const FSoftObjectPath& FallbackPath = nullptr)
	{
		// If the requested object is valid and loaded, early exit
		TObjectPtr<T> LoadedObject = SoftObjectPtr.Get();
		if (LoadedObject) { return LoadedObject; }

		// If not, make sure it's a valid path, and if not fallback to the fallback path
		FSoftObjectPath ToBeLoaded = SoftObjectPtr.ToSoftObjectPath().IsValid() ? SoftObjectPtr.ToSoftObjectPath() : FallbackPath;

		// Make sure we have a valid path at all
		if (!ToBeLoaded.IsValid()) { return nullptr; }

		// Check if the fallback path is loaded, early exit
		LoadedObject = TSoftObjectPtr<T>(ToBeLoaded).Get();
		if (LoadedObject) { return LoadedObject; }

		LoadBlocking_AnyThread(ToBeLoaded);

		return TSoftObjectPtr<T>(ToBeLoaded).Get();
	}

	PCGEXTENDEDTOOLKIT_API void LoadBlocking_AnyThread(const TSharedPtr<TSet<FSoftObjectPath>>& Paths);

	PCGEXTENDEDTOOLKIT_API void Load(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, FGetPaths&& GetPathsFunc, FOnLoadEnd&& OnLoadEnd);
}
