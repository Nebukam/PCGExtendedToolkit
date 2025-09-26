// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExContext.h"
#include "PCGExMT.h"
#include "PCGExSubSystem.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"


#include "Engine/AssetManager.h"

namespace PCGEx
{
	template <typename T>
	class TDiscoverAssetsTask;

	class PCGEXTENDEDTOOLKIT_API IAssetLoader : public TSharedFromThis<IAssetLoader>
	{
	public:
		IAssetLoader()
		{
		}
	};

	template <typename T>
	class TAssetLoader : public IAssetLoader
	{
	protected:
		bool bBypass = false;
		mutable FRWLock RegistrationLock;

		TArray<FName> AttributeNames;
		TSet<FSoftObjectPath> UniquePaths;

		TSharedPtr<FStreamableHandle> LoadHandle;
		TWeakPtr<PCGExMT::FAsyncToken> AsyncToken;

		int8 bEnded = 0;

	public:
		PCGExMT::FCompletionCallback OnComplete;
		FPCGExContext* Context = nullptr;
		TMap<FSoftObjectPath, TObjectPtr<T>> AssetsMap;
		TSharedRef<PCGExData::FPointIOCollection> IOCollection;

		TAssetLoader(
			FPCGExContext* InContext,
			const TSharedRef<PCGExData::FPointIOCollection>& InIOCollection,
			const TArray<FName>& InAttributeNames)
			: IAssetLoader(),
			  AttributeNames(InAttributeNames),
			  Context(InContext),
			  IOCollection(InIOCollection)
		{
		}

		~TAssetLoader()
		{
			Cancel();
		}


		bool HasEnded() const { return bEnded ? true : false; }

		TObjectPtr<T>* GetAsset(const FSoftObjectPath& Path) { return AssetsMap.Find(Path); }

		void AddExtraStructReferencedObjects(FReferenceCollector& Collector)
		{
			for (const TPair<FSoftObjectPath, TObjectPtr<T>>& Pair : AssetsMap)
			{
				TObjectPtr<UObject> Obj = Pair.Value;
				Collector.AddReferencedObject(Obj);
			}
		}

		bool Start(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
		{
			TArray<TSharedPtr<TDiscoverAssetsTask<T>>> Tasks;

			for (const TSharedPtr<PCGExData::FPointIO>& PointIO : IOCollection->Pairs)
			{
				TSharedRef<PCGExData::FPointIO> PointIORef = PointIO.ToSharedRef();
				for (const FName& AssetAttributeName : AttributeNames)
				{
					PCGEX_MAKE_SHARED(Broadcaster, TAttributeBroadcaster<FSoftObjectPath>)

					if (!Broadcaster->Prepare(AssetAttributeName, PointIORef))
					{
						// Warn & continue
						continue;
					}

					PCGEX_MAKE_SHARED(Task, TDiscoverAssetsTask<T>, SharedThis(this), Broadcaster)
					Tasks.Add(Task);
				}
			}

			if (Tasks.IsEmpty()) { return false; }

			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, AssetDiscovery)

			AsyncToken = AsyncManager->TryCreateToken(FName("AssetLoaderToken"));

			AssetDiscovery->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS

					if (!This->AsyncToken.IsValid()) { return; }

					PCGEX_SUBSYSTEM
					PCGExSubsystem->RegisterBeginTickAction(
						[AsyncThis]()
						{
							PCGEX_ASYNC_THIS
							This->Load();
						});
				};

			AssetDiscovery->StartTasksBatch(Tasks);

			return true;
		}

		void AddUniquePaths(const TSet<FSoftObjectPath>& InPaths)
		{
			FWriteScopeLock WriteScopeLock(RegistrationLock);
			UniquePaths.Append(InPaths);
		}

		bool Load()
		{
			if (UniquePaths.IsEmpty())
			{
				End(false);
				return false;
			}

			AssetsMap.Reserve(UniquePaths.Num());

			LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
				UniquePaths.Array(), [&]() { End(true); });

			if (!LoadHandle || !LoadHandle->IsActive())
			{
				if (!LoadHandle || !LoadHandle->HasLoadCompleted())
				{
					End(false);
					return false;
				}

				// Resources were already loaded
				End(true);
			}

			return true;
		}

		void Cancel()
		{
			if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
			LoadHandle.Reset();
			UniquePaths.Empty();
			End();
		}

		void End(const bool bBuildMap = false)
		{
			if (bEnded) { return; }

			FPlatformAtomics::InterlockedExchange(&bEnded, 1);

			if (bBuildMap)
			{
				for (FSoftObjectPath Path : UniquePaths)
				{
					TSoftObjectPtr<T> SoftPtr = TSoftObjectPtr<T>(Path);
					if (SoftPtr.Get()) { AssetsMap.Add(Path, SoftPtr.Get()); }
				}
			}

			if (AsyncToken.IsValid()) { AsyncToken.Pin()->Release(); }
			if (OnComplete) { OnComplete(); }
		}

		bool IsEmpty() { return AssetsMap.IsEmpty(); }
	};

	template <typename T>
	class TDiscoverAssetsTask final : public PCGExMT::FTask
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
			TSet<FSoftObjectPath> UniquePaths;
			Broadcaster->GrabUniqueValues(UniquePaths);

			TSet<FSoftObjectPath> UniqueValidPaths;
			for (const FSoftObjectPath& Path : UniquePaths)
			{
				if (!Path.IsAsset()) { continue; }
				UniqueValidPaths.Add(Path);
			}

			Loader->AddUniquePaths(UniqueValidPaths);
		}
	};
}
