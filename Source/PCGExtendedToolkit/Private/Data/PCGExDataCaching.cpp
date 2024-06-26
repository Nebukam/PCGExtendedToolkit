// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataCaching.h"

namespace PCGExDataCaching
{
	void FCacheBase::IncrementWriteReadyNum()
	{
		FWriteScopeLock WriteScopeLock(WriteLock);
		ReadyNum++;
	}

	void FCacheBase::ReadyWrite(PCGExMT::FTaskManager* AsyncManager)
	{
		FWriteScopeLock WriteScopeLock(WriteLock);
		ReadyNum--;
		if (ReadyNum <= 0) { Write(AsyncManager); }
	}

	void FCacheBase::Write(PCGExMT::FTaskManager* AsyncManager)
	{
	}

	FCacheBase* FPool::TryGetCache(const uint64 UID)
	{
		FReadScopeLock ReadScopeLock(PoolLock);
		FCacheBase** Found = CacheMap.Find(UID);
		if (!Found) { return nullptr; }
		return *Found;
	}
}
