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

	FCacheBase* FFacade::TryGetCache(const uint64 UID)
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

#pragma region Data forwarding

	FDataForwardHandler::~FDataForwardHandler()
	{
	}

	FDataForwardHandler::FDataForwardHandler(const FPCGExForwardDetails& InDetails, const FPointIO* InSourceIO):
		Details(InDetails), SourceIO(InSourceIO)
	{
		if (!Details.bEnabled) { return; }

		Details.Init();
		PCGEx::FAttributeIdentity::Get(InSourceIO->GetIn()->Metadata, Identities);
		Details.Filter(Identities);
	}

	void FDataForwardHandler::Forward(const int32 SourceIndex, const FPointIO* Target)
	{
		if (Identities.IsEmpty()) { return; }
		for (const PCGEx::FAttributeIdentity& Identity : Identities)
		{
			PCGMetadataAttribute::CallbackWithRightType(
				static_cast<uint16>(Identity.UnderlyingType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* SourceAtt = SourceIO->GetIn()->Metadata->GetConstTypedAttribute<T>(Identity.Name);
					Target->GetOut()->Metadata->DeleteAttribute(Identity.Name);
					FPCGMetadataAttribute<T>* Mark = Target->GetOut()->Metadata->FindOrCreateAttribute<T>(
						Identity.Name,
						SourceAtt->GetValueFromItemKey(SourceIO->GetInPoint(SourceIndex).MetadataEntry),
						SourceAtt->AllowsInterpolation(), true, true);
				});
		}
	}

#pragma endregion
}
