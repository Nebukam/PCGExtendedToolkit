// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "Data/PCGExValueHash.h"

namespace PCGExData
{
	class FPointIOCollection;
}

namespace PCGEx
{
	template <typename T>
	class TAttributeBroadcaster;
}

namespace PCGEx
{
	class PCGEXTENDEDTOOLKIT_API IAssetLoader : public TSharedFromThis<IAssetLoader>
	{
	protected:
		bool bBypass = false;
		mutable FRWLock RegistrationLock;

		TArray<FName> AttributeNames;
		TSet<FSoftObjectPath> UniquePaths;

		TSharedPtr<FStreamableHandle> LoadHandle;
		TWeakPtr<PCGExMT::FAsyncToken> AsyncToken;
		int8 bEnded = 0;

		FPCGExContext* Context = nullptr;

	public:
		PCGExMT::FCompletionCallback OnComplete;
		TSharedPtr<PCGExData::FPointIOCollection> IOCollection;
		TArray<TSharedPtr<TArray<PCGExValueHash>>> Keys;

		IAssetLoader() = default;
		IAssetLoader(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InIOCollection, const TArray<FName>& InAttributeNames);
		virtual ~IAssetLoader();

		virtual bool IsEmpty() {return true;}
		bool HasEnded() const { return bEnded ? true : false; }
		void Cancel();

		void AddUniquePaths(const TSet<FSoftObjectPath>& InPaths);

		bool Start(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);
		TSharedPtr<TArray<PCGExValueHash>> GetKeys(const int32 IOIndex);

		virtual bool Load();
		virtual void End(const bool bBuildMap = false) {}
		virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) {}

	protected:
		virtual void PrepareLoading();
	};

	template <typename T>
	class TAssetLoader : public IAssetLoader
	{
	public:
		TMap<PCGExValueHash, TObjectPtr<T>> AssetsMap;

		TAssetLoader(
			FPCGExContext* InContext,
			const TSharedPtr<PCGExData::FPointIOCollection>& InIOCollection,
			const TArray<FName>& InAttributeNames)
			: IAssetLoader(InContext, InIOCollection, InAttributeNames)
		{
		}

		virtual bool IsEmpty() override { return AssetsMap.IsEmpty(); }
		TObjectPtr<T>* GetAsset(const PCGExValueHash Key) { return AssetsMap.Find(Key); }

		virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override
		{
			for (const TPair<PCGExValueHash, TObjectPtr<T>>& Pair : AssetsMap)
			{
				TObjectPtr<UObject> Obj = Pair.Value;
				Collector.AddReferencedObject(Obj);
			}
		}

		virtual void End(const bool bBuildMap = false) override
		{
			if (bEnded) { return; }

			FPlatformAtomics::InterlockedExchange(&bEnded, 1);

			if (bBuildMap)
			{
				for (FSoftObjectPath Path : UniquePaths)
				{
					TSoftObjectPtr<T> SoftPtr = TSoftObjectPtr<T>(Path);
					if (SoftPtr.Get()) { AssetsMap.Add(PCGExBlend::ValueHash(Path), SoftPtr.Get()); }
				}
			}

			if (AsyncToken.IsValid()) { AsyncToken.Pin()->Release(); }
			if (OnComplete) { OnComplete(); }
		}

	protected:
		virtual void PrepareLoading() override
		{
			AssetsMap.Reserve(UniquePaths.Num());
		}
	};

	class FDiscoverAssetsTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(TDiscoverAssetsTask)

		FDiscoverAssetsTask(
			const TSharedPtr<IAssetLoader>& InLoader,
			const TSharedPtr<TAttributeBroadcaster<FSoftObjectPath>>& InBroadcaster);

		int32 IOIndex = -1;
		TSharedPtr<IAssetLoader> Loader;
		TSharedPtr<TAttributeBroadcaster<FSoftObjectPath>> Broadcaster;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}
