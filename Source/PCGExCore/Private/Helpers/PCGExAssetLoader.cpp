// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExAssetLoader.h"

#include "Core/PCGExContext.h"
#include "Types/PCGExTypes.h"
#include "Core/PCGExMT.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "Data/PCGExAttributeBroadcaster.h"
#include "Data/PCGExPointIO.h"
#include "Engine/AssetManager.h"

namespace PCGEx
{
	class FDiscoverAssetsTask final : public PCGExMT::FTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(TDiscoverAssetsTask)

		FDiscoverAssetsTask(const TSharedPtr<IAssetLoader>& InLoader, const TSharedPtr<PCGExData::TAttributeBroadcaster<FSoftObjectPath>>& InBroadcaster)
			: FTask(), Loader(InLoader), Broadcaster(InBroadcaster)
		{
		}

		int32 IOIndex = -1;
		TSharedPtr<IAssetLoader> Loader;
		TSharedPtr<PCGExData::TAttributeBroadcaster<FSoftObjectPath>> Broadcaster;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager) override
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

				Keys[i] = PCGExTypes::ComputeHash(Path);
				UniqueValidPaths.Add(Path);
			}

			UniqueValidPaths.Shrink();
			Loader->AddUniquePaths(UniqueValidPaths);
		}
	};

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
		PCGExHelpers::SafeReleaseHandle(LoadHandle);
		UniquePaths.Empty();
		End();
	}

	void IAssetLoader::AddUniquePaths(const TSet<FSoftObjectPath>& InPaths)
	{
		FWriteScopeLock WriteScopeLock(RegistrationLock);
		UniquePaths.Append(InPaths);
	}

	bool IAssetLoader::Start(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
	{
		TArray<TSharedPtr<FDiscoverAssetsTask>> Tasks;

		for (const TSharedPtr<PCGExData::FPointIO>& PointIO : IOCollection->Pairs)
		{
			TSharedRef<PCGExData::FPointIO> PointIORef = PointIO.ToSharedRef();
			for (const FName& AssetAttributeName : AttributeNames)
			{
				PCGEX_MAKE_SHARED(Broadcaster, PCGExData::TAttributeBroadcaster<FSoftObjectPath>)

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

		PCGEX_ASYNC_GROUP_CHKD(TaskManager, AssetDiscovery)

		AssetDiscovery->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE, TaskManager]()
		{
			PCGEX_ASYNC_THIS
			This->Load(TaskManager);
		};

		AssetDiscovery->StartTasksBatch(Tasks);

		return true;
	}

	TSharedPtr<TArray<PCGExValueHash>> IAssetLoader::GetKeys(const int32 IOIndex) { return Keys[IOIndex]; }

	bool IAssetLoader::Load(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
	{
		if (UniquePaths.IsEmpty())
		{
			End(false);
			return false;
		}

		LoadToken = TaskManager->TryCreateToken(FName("LoadToken"));
		PrepareLoading();

		PCGExHelpers::Load(
			TaskManager,
			[PCGEX_ASYNC_THIS_CAPTURE]() -> TArray<FSoftObjectPath>
			{
				PCGEX_ASYNC_THIS_RET({})
				return This->UniquePaths.Array();
			}, [PCGEX_ASYNC_THIS_CAPTURE](const bool bSuccess, TSharedPtr<FStreamableHandle> StreamableHandle)
			{
				PCGEX_ASYNC_THIS
				This->LoadHandle = StreamableHandle;
				This->End(bSuccess);
			});

		return true;
	}

	void IAssetLoader::End(const bool bBuildMap)
	{
		if (OnComplete) { OnComplete(); }
		PCGEX_ASYNC_RELEASE_TOKEN(LoadToken)
	}

	void IAssetLoader::PrepareLoading()
	{
	}
}
