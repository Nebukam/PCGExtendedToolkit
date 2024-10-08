// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

namespace PCGExData
{
#pragma region Pools & cache

	TSharedPtr<FBufferBase> FFacade::FindBufferUnsafe(const uint64 UID)
	{
		TSharedPtr<FBufferBase>* Found = BufferMap.Find(UID);
		if (!Found) { return nullptr; }
		return *Found;
	}

	TSharedPtr<FBufferBase> FFacade::FindBuffer(const uint64 UID)
	{
		FReadScopeLock ReadScopeLock(PoolLock);
		return FindBufferUnsafe(UID);
	}

#pragma endregion

#pragma region FIdxUnion

	void FFacade::Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable()) { return; }

		//UE_LOG(LogTemp, Warning, TEXT("{%lld} Facade -> Write"), AsyncManager->Context->GetInputSettings<UPCGSettings>()->UID)

		for (const TSharedPtr<FBufferBase> Buffer : Buffers)
		{
			if (!Buffer.IsValid() || !Buffer->IsWritable()) { continue; }
			PCGExMT::Write(AsyncManager, Buffer);
		}

		Flush();
	}

	void FFacade::WriteBuffersAsCallbacks(const TSharedPtr<PCGExMT::FTaskGroup>& TaskGroup)
	{
		// !!! Requires manual flush !!!

		if (!TaskGroup)
		{
			Flush();
			return;
		}

		for (const TSharedPtr<FBufferBase> Buffer : Buffers)
		{
			if (Buffer->IsWritable()) { TaskGroup->AddSimpleCallback([BufferRef = Buffer]() { BufferRef->Write(); }); }
		}
	}

	void FUnionData::ComputeWeights(
		const TArray<TSharedPtr<FFacade>>& Sources, const TMap<uint32, int32>& SourcesIdx, const FPCGPoint& Target,
		const FPCGExDistanceDetails& InDistanceDetails, TArray<int32>& OutIOIdx, TArray<int32>& OutPointsIdx, TArray<double>& OutWeights) const
	{
		const int32 NumHashes = ItemHashSet.Num();

		OutPointsIdx.SetNumUninitialized(NumHashes);
		OutWeights.SetNumUninitialized(NumHashes);
		OutIOIdx.SetNumUninitialized(NumHashes);

		double TotalWeight = 0;
		int32 Index = 0;

		for (const uint64 Hash : ItemHashSet)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64(Hash, IOIndex, PtIndex);

			const int32* IOIdx = SourcesIdx.Find(IOIndex);
			if (!IOIdx) { continue; }

			OutIOIdx[Index] = *IOIdx;
			OutPointsIdx[Index] = PtIndex;

			const double Weight = InDistanceDetails.GetDistance(Sources[*IOIdx]->Source->GetInPoint(PtIndex), Target);
			OutWeights[Index] = Weight;
			TotalWeight += Weight;

			Index++;
		}

		if (Index == 0) { return; }

		OutPointsIdx.SetNum(Index);
		OutWeights.SetNum(Index);
		OutIOIdx.SetNum(Index);

		if (Index == 1)
		{
			OutWeights[0] = 1;
			return;
		}

		if (TotalWeight == 0)
		{
			const double StaticWeight = 1 / static_cast<double>(ItemHashSet.Num());
			for (double& Weight : OutWeights) { Weight = StaticWeight; }
			return;
		}

		for (double& Weight : OutWeights) { Weight = 1 - (Weight / TotalWeight); }
	}

	uint64 FUnionData::Add(const int32 IOIndex, const int32 PointIndex)
	{
		const uint64 H = PCGEx::H64(IOIndex, PointIndex);

		{
			FWriteScopeLock WriteScopeLock(UnionLock);
			IOIndices.Add(IOIndex);
			ItemHashSet.Add(H);
		}

		return H;
	}

	FUnionData* FUnionMetadata::NewEntry(const int32 IOIndex, const int32 ItemIndex)
	{
		FUnionData* NewUnionData = Entries.Add_GetRef(new FUnionData());
		//NewUnionData->Index = Items.Num() - 1;
		NewUnionData->IOIndices.Add(IOIndex);
		const uint64 H = PCGEx::H64(IOIndex, ItemIndex);
		NewUnionData->ItemHashSet.Add(H);

		return NewUnionData;
	}

	uint64 FUnionMetadata::Append(const int32 Index, const int32 IOIndex, const int32 ItemIndex)
	{
		return Entries[Index]->Add(IOIndex, ItemIndex);
	}

	bool FUnionMetadata::IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
	{
		const TSet<int32> Overlap = Entries[InIdx]->IOIndices.Intersect(InIndices);
		return Overlap.Num() > 0;
	}


#pragma endregion
}
