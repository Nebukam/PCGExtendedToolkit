// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

namespace PCGExData
{
#pragma region Pools & cache

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

	FCacheBase* FFacade::FindCache(const uint64 UID)
	{
		FReadScopeLock ReadScopeLock(PoolLock);
		FCacheBase** Found = CacheMap.Find(UID);
		if (!Found) { return nullptr; }
		return *Found;
	}

#pragma endregion

#pragma region FIdxCompound

	void FIdxCompound::ComputeWeights(
		const TArray<FFacade*>& Sources, const TMap<uint32, int32>& SourcesIdx, const FPCGPoint& Target,
		const FPCGExDistanceDetails& InDistanceDetails, TArray<uint64>& OutCompoundHashes, TArray<double>& OutWeights)
	{
		OutCompoundHashes.SetNumUninitialized(CompoundedHashSet.Num());
		OutWeights.SetNumUninitialized(CompoundedHashSet.Num());

		double TotalWeight = 0;
		int32 Index = 0;
		for (const uint64 Hash : CompoundedHashSet)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64(Hash, IOIndex, PtIndex);

			const int32* IOIdx = SourcesIdx.Find(IOIndex);
			if (!IOIdx) { continue; }

			OutCompoundHashes[Index] = Hash;

			const double Weight = InDistanceDetails.GetDistance(Sources[*IOIdx]->Source->GetInPoint(PtIndex), Target);
			OutWeights[Index] = Weight;
			TotalWeight += Weight;

			Index++;
		}

		if (TotalWeight == 0)
		{
			const double StaticWeight = 1 / static_cast<double>(CompoundedHashSet.Num());
			for (double& Weight : OutWeights) { Weight = StaticWeight; }
			return;
		}

		for (double& Weight : OutWeights) { Weight = 1 - (Weight / TotalWeight); }
	}

	uint64 FIdxCompound::Add(const int32 IOIndex, const int32 PointIndex)
	{
		IOIndices.Add(IOIndex);
		const uint64 H = PCGEx::H64(IOIndex, PointIndex);
		CompoundedHashSet.Add(H);
		return H;
	}


#pragma endregion
}
