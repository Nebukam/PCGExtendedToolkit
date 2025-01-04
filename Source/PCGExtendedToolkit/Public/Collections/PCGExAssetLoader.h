// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExContext.h"
#include "PCGExMT.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"


#include "Engine/AssetManager.h"

namespace PCGEx
{
	PCGEX_CTX_STATE(InternalState_DiscoveringAssets)
	PCGEX_CTX_STATE(InternalState_LoadingAssets)
	PCGEX_CTX_STATE(InternalState_AssetsLoaded)

	template <typename T>
	class TDiscoverAssetsTask;

	class /*PCGEXTENDEDTOOLKIT_API*/ TAssetLoaderBase : public TSharedFromThis<TAssetLoaderBase>
	{
	public:
		TAssetLoaderBase()
		{
		}
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TAssetLoader : public TAssetLoaderBase
	{
	protected:
		bool bBypass = false;
		mutable FRWLock RegistrationLock;

		TArray<FName> AttributeNames;
		ContextState ExitState = State_WaitingOnAsyncWork;

		TSet<FSoftObjectPath> UniquePaths;
		TSharedPtr<FStreamableHandle> LoadHandle;

	public:
		FPCGExContext* Context = nullptr;
		TMap<FSoftObjectPath, TObjectPtr<T>> AssetsMap;
		TSharedRef<PCGExData::FPointIOCollection> IOCollection;

		TAssetLoader(
			FPCGExContext* InContext,
			const TSharedRef<PCGExData::FPointIOCollection>& InIOCollection,
			const TArray<FName>& InAttributeNames)
			: TAssetLoaderBase(),
			  AttributeNames(InAttributeNames),
			  Context(InContext),
			  IOCollection(InIOCollection)
		{
		}

		~TAssetLoader()
		{
			Cancel();
		}

		FORCEINLINE TObjectPtr<T>* GetAsset(const FSoftObjectPath& Path) { return AssetsMap.Find(Path); }

		bool Start(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager, const ContextState InExitState)
		{
			ExitState = InExitState;
			Context->SetAsyncState(InternalState_DiscoveringAssets);

			bool bAnyDiscovery = false;

			for (const TSharedPtr<PCGExData::FPointIO>& PointIO : IOCollection->Pairs)
			{
				TSharedRef<PCGExData::FPointIO> PointIORef = PointIO.ToSharedRef();
				for (const FName& AssetAttributeName : AttributeNames)
				{
#if PCGEX_ENGINE_VERSION <= 503
					PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<FString>)
#else
					PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<FSoftObjectPath>)
#endif

					if (!Broadcaster->Prepare(AssetAttributeName, PointIORef))
					{
						// Warn & continue
						continue;
					}

					bAnyDiscovery = true;

					PCGEX_LAUNCH(TDiscoverAssetsTask<T>, SharedThis(this), Broadcaster)
				}
			}

			return bAnyDiscovery;
		}

		void AddUniquePaths(const TSet<FSoftObjectPath>& InPaths)
		{
			FWriteScopeLock WriteScopeLock(RegistrationLock);
			UniquePaths.Append(InPaths);
		}

		bool Load(const bool bForceSynchronous = false)
		{
			if (UniquePaths.IsEmpty()) { return false; }

			AssetsMap.Reserve(UniquePaths.Num());

			Context->SetAsyncState(InternalState_LoadingAssets);

			if (!bForceSynchronous)
			{
				Context->PauseContext();
				LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
					UniquePaths.Array(), [&]()
					{
						Context->SetAsyncState(InternalState_AssetsLoaded);
						Context->ResumeExecution();
					});

				if (!LoadHandle || !LoadHandle->IsActive())
				{
					if (!LoadHandle || !LoadHandle->HasLoadCompleted())
					{
						Context->CancelExecution("Error loading assets.");
						return false;
					}

					// Resources were already loaded
					Context->SetAsyncState(InternalState_AssetsLoaded);
					Context->ResumeExecution();
					return true;
				}
			}
			else
			{
				LoadHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(UniquePaths.Array());
				Context->SetAsyncState(InternalState_AssetsLoaded);
			}

			return true;
		}

		void Cancel()
		{
			if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
			LoadHandle.Reset();
			UniquePaths.Empty();
		}

		bool Execute()
		{
			if (bBypass) { return true; }

			PCGEX_ON_ASYNC_STATE_READY(InternalState_DiscoveringAssets)
			{
				if (!Load())
				{
					Context->CancelExecution(TEXT("Loading resources failed"));
					return false;
				}

				return false;
			}

			if (Context->IsState(InternalState_LoadingAssets)) { return false; }

			PCGEX_ON_ASYNC_STATE_READY(InternalState_AssetsLoaded)
			{
				bBypass = true;

				// Build asset map
				for (FSoftObjectPath Path : UniquePaths)
				{
					TSoftObjectPtr<T> SoftPtr = TSoftObjectPtr<T>(Path);
					if (SoftPtr.Get()) { AssetsMap.Add(Path, SoftPtr.Get()); }
				}

				Context->SetState(ExitState);
			}

			return true;
		}
	};

#if PCGEX_ENGINE_VERSION <= 503
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDiscoverAssetsTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(TDiscoverAssetsTask)
		
		TDiscoverAssetsTask(const TSharedPtr<TAssetLoader<T>>& InLoader,
		                    const TSharedPtr<TAttributeBroadcaster<FString>>& InBroadcaster) :
			FTask(),
			Loader(InLoader),
			Broadcaster(InBroadcaster)
		{
		}

		TSharedPtr<TAssetLoader<T>> Loader;
		TSharedPtr<TAttributeBroadcaster<FString>> Broadcaster;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			Broadcaster->Grab(false);

			TSet<FSoftObjectPath> UniquePaths;
			for (const FString& Path : Broadcaster->Values)
			{
				FSoftObjectPath PathTemp = FSoftObjectPath(Path);
				if (!PathTemp.IsAsset()) { continue; }
				UniquePaths.Add(PathTemp);
			}

			Loader->AddUniquePaths(UniquePaths);
		}
	};
#else
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDiscoverAssetsTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(TDiscoverAssetsTask)

		TDiscoverAssetsTask(const TSharedPtr<TAssetLoader<T>>& InLoader,
		                    const TSharedPtr<TAttributeBroadcaster<FSoftObjectPath>>& InBroadcaster) :
			FTask(),
			Loader(InLoader),
			Broadcaster(InBroadcaster)
		{
		}

		TSharedPtr<TAssetLoader<T>> Loader;
		TSharedPtr<TAttributeBroadcaster<FSoftObjectPath>> Broadcaster;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			Broadcaster->Grab(false);

			TSet<FSoftObjectPath> UniquePaths;
			for (const FSoftObjectPath& Path : Broadcaster->Values)
			{
				if (!Path.IsAsset()) { continue; }
				UniquePaths.Add(Path);
			}

			Loader->AddUniquePaths(UniquePaths);
		}
	};
#endif
}
