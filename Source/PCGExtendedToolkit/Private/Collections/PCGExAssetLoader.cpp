// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Collections/PCGExAssetLoader.h"

#include "PCGExContext.h"
#include "PCGExSubSystem.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Engine/AssetManager.h"

namespace PCGEx
{
	IAssetLoader::IAssetLoader(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InIOCollection, const TArray<FName>& InAttributeNames)
		: AttributeNames(InAttributeNames), Context(InContext), IOCollection(InIOCollection)
	{
		Keys.Init(nullptr, InIOCollection->Num());
	}

	IAssetLoader::~IAssetLoader()
	{
		Cancel();
	}

	void IAssetLoader::Cancel()
	{
		if (LoadHandle.IsValid() && LoadHandle->IsActive()) { LoadHandle->CancelHandle(); }
		LoadHandle.Reset();
		UniquePaths.Empty();
		End();
	}

	void IAssetLoader::AddUniquePaths(const TSet<FSoftObjectPath>& InPaths)
	{
		FWriteScopeLock WriteScopeLock(RegistrationLock);
		UniquePaths.Append(InPaths);
	}

	bool IAssetLoader::Start(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TArray<TSharedPtr<FDiscoverAssetsTask>> Tasks;

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

				PCGEX_MAKE_SHARED(Task, FDiscoverAssetsTask, SharedThis(this), Broadcaster)
				Task->IOIndex = PointIORef->IOIndex;
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

	TSharedPtr<TArray<PCGExValueHash>> IAssetLoader::GetKeys(const int32 IOIndex) { return Keys[IOIndex]; }

	bool IAssetLoader::Load()
	{
		if (UniquePaths.IsEmpty())
		{
			End(false);
			return false;
		}

		PrepareLoading();

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

	void IAssetLoader::PrepareLoading()
	{
	}

	FDiscoverAssetsTask::FDiscoverAssetsTask(
		const TSharedPtr<IAssetLoader>& InLoader,
		const TSharedPtr<TAttributeBroadcaster<FSoftObjectPath>>& InBroadcaster)
		: FTask(),
		  Loader(InLoader),
		  Broadcaster(InBroadcaster)
	{
	}

	void FDiscoverAssetsTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FSoftObjectPath Min;
		FSoftObjectPath Max;
		TArray<FSoftObjectPath> ValueDump;
		Broadcaster->GrabAndDump(ValueDump, false, Min, Max);

		const int32 NumValues = ValueDump.Num();

		TSharedPtr<TArray<PCGExValueHash>> KeysPtrs = MakeShared<TArray<PCGExValueHash>>();
		KeysPtrs->Init(0, NumValues);

		Loader->Keys[IOIndex] = KeysPtrs;
		TArray<PCGExValueHash>& Keys = *KeysPtrs.Get();

		TSet<FSoftObjectPath> UniqueValidPaths;
		UniqueValidPaths.Reserve(NumValues);

		for (int i = 0; i < NumValues; i++)
		{
			const FSoftObjectPath& Path = ValueDump[i];
			if (!Path.IsAsset()) { continue; }

			Keys[i] = PCGExBlend::ValueHash(Path);
			UniqueValidPaths.Add(Path);
		}

		UniqueValidPaths.Shrink();
		Loader->AddUniquePaths(UniqueValidPaths);
	}
}
