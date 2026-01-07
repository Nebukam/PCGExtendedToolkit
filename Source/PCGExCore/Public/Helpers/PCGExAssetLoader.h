// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Core/PCGExMTCommon.h"
#include "Misc/ScopeRWLock.h"
#include "Types/PCGExTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/SoftObjectPath.h"

struct FPCGExContext;

namespace PCGExMT
{
	class FAsyncToken;
	class FTaskManager;
}

struct FStreamableHandle;

namespace PCGExData
{
	class FPointIOCollection;
}

namespace PCGExData
{
	template <typename T>
	class TAttributeBroadcaster;
}

namespace PCGEx
{
	class PCGEXCORE_API IAssetLoader : public TSharedFromThis<IAssetLoader>
	{
	protected:
		bool bBypass = false;
		mutable FRWLock RegistrationLock;

		TArray<FName> AttributeNames;
		TSet<FSoftObjectPath> UniquePaths;

		TWeakPtr<PCGExMT::FAsyncToken> LoadToken;
		TSharedPtr<FStreamableHandle> LoadHandle;
		int8 bEnded = 0;

		FPCGExContext* Context = nullptr;

	public:
		PCGExMT::FCompletionCallback OnComplete;
		TSharedPtr<PCGExData::FPointIOCollection> IOCollection;
		TArray<TSharedPtr<TArray<PCGExValueHash>>> Keys;

		IAssetLoader() = default;
		IAssetLoader(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InIOCollection, const TArray<FName>& InAttributeNames);
		virtual ~IAssetLoader();

		virtual bool IsEmpty() { return true; }
		bool HasEnded() const { return bEnded ? true : false; }
		void Cancel();

		void AddUniquePaths(const TSet<FSoftObjectPath>& InPaths);

		bool Start(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager);
		TSharedPtr<TArray<PCGExValueHash>> GetKeys(const int32 IOIndex);

		virtual bool Load(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager);

		virtual void End(const bool bBuildMap = false);

	protected:
		virtual void PrepareLoading();
	};

	template <typename T>
	class TAssetLoader : public IAssetLoader
	{
	public:
		TMap<PCGExValueHash, TObjectPtr<T>> AssetsMap;

		TAssetLoader(FPCGExContext* InContext, const TSharedPtr<PCGExData::FPointIOCollection>& InIOCollection, const TArray<FName>& InAttributeNames)
			: IAssetLoader(InContext, InIOCollection, InAttributeNames)
		{
		}

		virtual bool IsEmpty() override { return AssetsMap.IsEmpty(); }
		TObjectPtr<T>* GetAsset(const PCGExValueHash Key) { return AssetsMap.Find(Key); }

		virtual void End(const bool bBuildMap = false) override
		{
			if (bEnded) { return; }

			FPlatformAtomics::InterlockedExchange(&bEnded, 1);

			if (bBuildMap)
			{
				for (FSoftObjectPath Path : UniquePaths)
				{
					TSoftObjectPtr<T> SoftPtr = TSoftObjectPtr<T>(Path);
					if (SoftPtr.Get()) { AssetsMap.Add(PCGExTypes::ComputeHash(Path), SoftPtr.Get()); }
				}
			}

			IAssetLoader::End();
		}

	protected:
		virtual void PrepareLoading() override
		{
			AssetsMap.Reserve(UniquePaths.Num());
		}
	};
}
