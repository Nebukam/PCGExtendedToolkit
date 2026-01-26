// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/PCGExClusterCache.h"

namespace PCGExClusters
{
#pragma region FClusterCacheRegistry

	FClusterCacheRegistry& FClusterCacheRegistry::Get()
	{
		static FClusterCacheRegistry Instance;
		return Instance;
	}

	void FClusterCacheRegistry::Register(const TSharedRef<IClusterCacheFactory>& Factory)
	{
		FWriteScopeLock WriteLock(Lock);
		Factories.Add(Factory->GetCacheKey(), Factory);
	}

	void FClusterCacheRegistry::Unregister(FName Key)
	{
		FWriteScopeLock WriteLock(Lock);
		Factories.Remove(Key);
	}

	IClusterCacheFactory* FClusterCacheRegistry::GetFactory(FName Key) const
	{
		FReadScopeLock ReadLock(Lock);
		if (const TSharedRef<IClusterCacheFactory>* Found = Factories.Find(Key))
		{
			return &Found->Get();
		}
		return nullptr;
	}

	void FClusterCacheRegistry::GetPreBuildKeys(TArray<FName>& OutKeys) const
	{
		FReadScopeLock ReadLock(Lock);
		for (const auto& Pair : Factories)
		{
			if (Pair.Value->GetCacheType() == EClusterCacheType::PreBuild)
			{
				OutKeys.Add(Pair.Key);
			}
		}
	}

	void FClusterCacheRegistry::GetOpportunisticKeys(TArray<FName>& OutKeys) const
	{
		FReadScopeLock ReadLock(Lock);
		for (const auto& Pair : Factories)
		{
			if (Pair.Value->GetCacheType() == EClusterCacheType::Opportunistic)
			{
				OutKeys.Add(Pair.Key);
			}
		}
	}

	void FClusterCacheRegistry::GetAllFactories(TArray<TSharedRef<IClusterCacheFactory>>& OutFactories) const
	{
		FReadScopeLock ReadLock(Lock);
		Factories.GenerateValueArray(OutFactories);
	}

#pragma endregion
}
