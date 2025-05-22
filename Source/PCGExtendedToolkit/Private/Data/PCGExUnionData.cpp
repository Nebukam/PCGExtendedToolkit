// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExUnionData.h"

#include "PCGExPointsMT.h"

namespace PCGExData
{
#pragma region Union Data

	int32 FUnionData::ComputeWeights(
		const TArray<const UPCGBasePointData*>& Sources, const TMap<uint32, int32>& SourcesIdx, const FConstPoint& Target,
		const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const
	{
		const int32 NumHashes = ItemHashSet.Num();

		OutWeightedPoints.Reset(NumHashes);

		double TotalWeight = 0;
		int32 Index = 0;

		for (const uint64 Hash : ItemHashSet)
		{
			uint32 IOIndex;
			uint32 PtIndex;
			PCGEx::H64(Hash, IOIndex, PtIndex);

			const int32* IOIdx = SourcesIdx.Find(IOIndex);
			if (!IOIdx) { continue; }

			FWeightedPoint& P = OutWeightedPoints.Emplace_GetRef(PtIndex, 0, *IOIdx);

			const double Weight = InDistanceDetails->GetDistSquared(FConstPoint(Sources[P.IO], P), Target);
			P.Weight = Weight;
			TotalWeight += Weight;

			Index++;
		}

		if (Index == 0) { return 0; }

		if (Index == 1)
		{
			OutWeightedPoints[0].Weight = 1;
			return 1;
		}

		if (TotalWeight == 0)
		{
			const double StaticWeight = 1 / static_cast<double>(Index);
			for (FWeightedPoint& P : OutWeightedPoints) { P.Weight = StaticWeight; }
			return Index;
		}

		for (FWeightedPoint& P : OutWeightedPoints) { P.Weight = 1 - (P.Weight / TotalWeight); }
		return Index;
	}

	uint64 FUnionData::Add(const PCGExData::FPoint& Point)
	{
		const uint64 H = PCGEx::H64(Point.IO, Point.Index == -1 ? 0 : Point.Index);

		{
			FWriteScopeLock WriteScopeLock(UnionLock);
			IOIndices.Add(Point.IO);
			ItemHashSet.Add(H);
		}

		return H;
	}

	void FUnionData::Add(const int32 IOIndex, const TArray<int32>& PointIndices)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);

		IOIndices.Add(IOIndex);
		for (const int32 A : PointIndices) { ItemHashSet.Add(PCGEx::H64(IOIndex, A)); }
	}

	void FUnionMetadata::SetNum(const int32 InNum)
	{
		// To be used only with NewEntryAt / NewEntryAt_Unsafe
		Entries.Init(nullptr, InNum);
	}

	TSharedPtr<FUnionData> FUnionMetadata::NewEntry_Unsafe(const PCGExData::FConstPoint& Point)
	{
		TSharedPtr<FUnionData> NewUnionData = Entries.Add_GetRef(MakeShared<FUnionData>());
		NewUnionData->Add(Point);
		return NewUnionData;
	}

	TSharedPtr<FUnionData> FUnionMetadata::NewEntryAt_Unsafe(const int32 ItemIndex)
	{
		Entries[ItemIndex] = MakeShared<FUnionData>();
		return Entries[ItemIndex];
	}

	uint64 FUnionMetadata::Append(const int32 Index, const PCGExData::FPoint& Point)
	{
		return Entries[Index]->Add(Point);
	}

	bool FUnionMetadata::IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
	{
		const TSet<int32> Overlap = Entries[InIdx]->IOIndices.Intersect(InIndices);
		return Overlap.Num() > 0;
	}

	TSharedPtr<FFacade> TryGetSingleFacade(FPCGExContext* InContext, const FName InputPinLabel, const bool bTransactional, const bool bThrowError)
	{
		TSharedPtr<FFacade> SingleFacade;
		if (const TSharedPtr<FPointIO> SingleIO = TryGetSingleInput(InContext, InputPinLabel, bTransactional, bThrowError))
		{
			SingleFacade = MakeShared<FFacade>(SingleIO.ToSharedRef());
		}

		return SingleFacade;
	}

	bool TryGetFacades(FPCGExContext* InContext, const FName InputPinLabel, TArray<TSharedPtr<FFacade>>& OutFacades, const bool bThrowError, const bool bIsTransactional)
	{
		TSharedPtr<FPointIOCollection> TargetsCollection = MakeShared<FPointIOCollection>(InContext, InputPinLabel, EIOInit::None, bIsTransactional);
		if (TargetsCollection->IsEmpty())
		{
			if (bThrowError) { PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FText::FromString(TEXT("Missing or zero-points '{0}' inputs")), FText::FromName(InputPinLabel))); }
			return false;
		}

		OutFacades.Reserve(OutFacades.Num() + TargetsCollection->Num());
		for (const TSharedPtr<FPointIO>& IO : TargetsCollection->Pairs) { OutFacades.Add(MakeShared<FFacade>(IO.ToSharedRef())); }

		return true;
	}


#pragma endregion
}
