// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExData.h"

#include "PCGExPointsMT.h"

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

	void FFacade::MarkCurrentBuffersReadAsComplete()
	{
		for (const TSharedPtr<FBufferBase>& Buffer : Buffers)
		{
			if (!Buffer.IsValid() || !Buffer->IsReadable()) { continue; }
			Buffer->bReadComplete = true;
		}
	}

	void FFacade::Write(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (!AsyncManager || !AsyncManager->IsAvailable() || !Source->GetOut()) { return; }

		//UE_LOG(LogTemp, Warning, TEXT("{%lld} Facade -> Write"), AsyncManager->Context->GetInputSettings<UPCGSettings>()->UID)

		Source->GetOutKeys(true);

		for (int i = 0; i < Buffers.Num(); i++)
		{
			const TSharedPtr<FBufferBase> Buffer = Buffers[i];
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

		Source->GetOutKeys(true);

		for (const TSharedPtr<FBufferBase>& Buffer : Buffers)
		{
			if (Buffer->IsWritable()) { TaskGroup->AddSimpleCallback([BufferRef = Buffer]() { BufferRef->Write(); }); }
		}
	}

	bool FReadableBufferConfig::Validate(FPCGExContext* InContext, const TSharedRef<FFacade>& InFacade) const
	{
		return true;
	}

	void FReadableBufferConfig::Fetch(const TSharedRef<FFacade>& InFacade, const int32 StartIndex, const int32 Count) const
	{
		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<TBuffer<T>> Reader = nullptr;
				switch (Mode)
				{
				case EBufferPreloadType::RawAttribute:
					Reader = InFacade->GetScopedReadable<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromName:
					Reader = InFacade->GetScopedBroadcaster<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromSelector:
					Reader = InFacade->GetScopedBroadcaster<T>(Selector);
					break;
				}
				Reader->Fetch(StartIndex, Count);
			});
	}

	void FReadableBufferConfig::Read(const TSharedRef<FFacade>& InFacade) const
	{
		PCGEx::ExecuteWithRightType(
			Identity.UnderlyingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<TBuffer<T>> Reader = nullptr;
				switch (Mode)
				{
				case EBufferPreloadType::RawAttribute:
					Reader = InFacade->GetReadable<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromName:
					Reader = InFacade->GetBroadcaster<T>(Identity.Name);
					break;
				case EBufferPreloadType::BroadcastFromSelector:
					Reader = InFacade->GetBroadcaster<T>(Selector);
					break;
				}
			});
	}

	bool FFacadePreloader::Validate(FPCGExContext* InContext, const TSharedRef<FFacade>& InFacade) const
	{
		if (BufferConfigs.IsEmpty()) { return true; }
		for (const FReadableBufferConfig& Config : BufferConfigs) { if (!Config.Validate(InContext, InFacade)) { return false; } }
		return true;
	}

	void FFacadePreloader::Fetch(const TSharedRef<FFacade>& InFacade, const int32 StartIndex, const int32 Count) const
	{
		for (const FReadableBufferConfig& ExistingConfig : BufferConfigs) { ExistingConfig.Fetch(InFacade, StartIndex, Count); }
	}

	void FFacadePreloader::Read(const TSharedRef<FFacade>& InFacade, const int32 ConfigIndex) const
	{
		BufferConfigs[ConfigIndex].Read(InFacade);
	}

	void FFacadePreloader::StartLoading(TSharedPtr<PCGExMT::FTaskManager> AsyncManager, const TSharedRef<FFacade>& InDataFacade, TSharedPtr<PCGExMT::FTaskGroup> InTaskGroup)
	{
		InternalDataFacadePtr = InDataFacade;
		TaskGroupPtr = InTaskGroup;

		if (!IsEmpty())
		{
			if (!Validate(AsyncManager->Context, InDataFacade))
			{
				InternalDataFacadePtr.Reset();
				TaskGroupPtr.Reset();
				OnLoadingEnd();
				return;
			}

			PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PrefetchAttributesTask)

			if (InTaskGroup) { InTaskGroup->GrowNumStarted(); }

			PrefetchAttributesTask->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->OnLoadingEnd();
				};

			if (InDataFacade->bSupportsScopedGet)
			{
				PrefetchAttributesTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						PCGEX_ASYNC_THIS
						if (const TSharedPtr<FFacade> InternalFacade = This->InternalDataFacadePtr.Pin())
						{
							This->Fetch(InternalFacade.ToSharedRef(), StartIndex, Count);
						}
					};

				PrefetchAttributesTask->StartSubLoops(InDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
			}
			else
			{
				PrefetchAttributesTask->OnSubLoopStartCallback =
					[PCGEX_ASYNC_THIS_CAPTURE]
					(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
					{
						PCGEX_ASYNC_THIS
						if (const TSharedPtr<FFacade> InternalFacade = This->InternalDataFacadePtr.Pin())
						{
							This->Read(InternalFacade.ToSharedRef(), StartIndex);
						}
					};

				PrefetchAttributesTask->StartSubLoops(Num(), 1);
			}
		}
		else
		{
			TaskGroupPtr.Reset();
			OnLoadingEnd();
		}
	}

	void FFacadePreloader::OnLoadingEnd() const
	{
		if (TSharedPtr<FFacade> InternalFacade = InternalDataFacadePtr.Pin()) { InternalFacade->MarkCurrentBuffersReadAsComplete(); }
		if (OnCompleteCallback) { OnCompleteCallback(); }
		if (const TSharedPtr<PCGExMT::FTaskGroup> TaskGroup = TaskGroupPtr.Pin()) { TaskGroup->GrowNumCompleted(); }
	}

	void FUnionData::ComputeWeights(
		const TArray<TSharedPtr<FFacade>>& Sources, const TMap<uint32, int32>& SourcesIdx, const FPCGPoint& Target,
		const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<int32>& OutIOIdx, TArray<int32>& OutPointsIdx, TArray<double>& OutWeights) const
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

			const double Weight = InDistanceDetails->GetDistance(Sources[*IOIdx]->Source->GetInPoint(PtIndex), Target);
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

	TSharedPtr<FUnionData> FUnionMetadata::NewEntry(const int32 IOIndex, const int32 ItemIndex)
	{
		TSharedPtr<FUnionData> NewUnionData = Entries.Add_GetRef(MakeShared<FUnionData>());
		NewUnionData->Add(IOIndex, ItemIndex);
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
