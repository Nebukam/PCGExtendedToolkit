﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExUnionData.h"

#include "PCGExPointsMT.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExDetailsDistances.h"

namespace PCGExData
{
#pragma region Union Data

	void IUnionData::Add(const FElement& Point)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);
		Add_Unsafe(Point.Index, Point.IO);
	}

	void IUnionData::Add(const int32 Index, const int32 IO)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);
		Add_Unsafe(Index, IO);
	}

	void IUnionData::Add_Unsafe(const int32 IOIndex, const TArray<int32>& PointIndices)
	{
		IOSet.Add(IOIndex);
		Elements.Reserve(Elements.Num() + PointIndices.Num());
		for (const int32 A : PointIndices) { Elements.Add(FElement(A, IOIndex)); }
	}

	void IUnionData::Add(const int32 IOIndex, const TArray<int32>& PointIndices)
	{
		FWriteScopeLock WriteScopeLock(UnionLock);
		Add_Unsafe(IOIndex, PointIndices);
	}

	int32 IUnionData::ComputeWeights(
		const TArray<const UPCGBasePointData*>& Sources,
		const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
		const FPoint& Target,
		const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
		TArray<FWeightedPoint>& OutWeightedPoints) const
	{
		const int32 NumElements = Elements.Num();
		OutWeightedPoints.Reset(NumElements);

		double MaxWeight = 0;
		double TotalWeight = 0;
		int32 Index = 0;

		for (const FElement& Element : Elements)
		{
			const int32 IOIdx = IdxLookup->Get(Element.IO);
			if (IOIdx == -1) { continue; }

			FWeightedPoint& P = OutWeightedPoints.Emplace_GetRef(Element.Index, 0, IOIdx);
			const double Dist = InDistanceDetails->GetDistSquared(FConstPoint(Sources[P.IO], P), Target);
			P.Weight = Dist;

			MaxWeight = FMath::Max(MaxWeight, Dist);
			Index++;
		}

		if (Index == 0) { return 0; }

		// Normalize & one minus distances to make them weights
		for (FWeightedPoint& P : OutWeightedPoints) { TotalWeight += (P.Weight = 1 - (P.Weight / MaxWeight)); }

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

		// Normalize weights
		//for (FWeightedPoint& P : OutWeightedPoints) { P.Weight /= TotalWeight; }
		return Index;
	}

	void FUnionMetadata::SetNum(const int32 InNum)
	{
		// To be used only with NewEntryAt / NewEntryAt_Unsafe
		Entries.Init(nullptr, InNum);
	}

	TSharedPtr<IUnionData> FUnionMetadata::NewEntry_Unsafe(const FConstPoint& Point)
	{
		TSharedPtr<IUnionData> NewUnionData = Entries.Add_GetRef(MakeShared<IUnionData>());
		NewUnionData->Add_Unsafe(Point);
		return NewUnionData;
	}

	TSharedPtr<IUnionData> FUnionMetadata::NewEntryAt_Unsafe(const int32 ItemIndex)
	{
		Entries[ItemIndex] = MakeShared<IUnionData>();
		return Entries[ItemIndex];
	}

	bool FUnionMetadata::IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices)
	{
		const TSet<int32> Overlap = Entries[InIdx]->IOSet.Intersect(InIndices);
		return Overlap.Num() > 0;
	}

	TSharedPtr<FFacade> TryGetSingleFacade(FPCGExContext* InContext, const FName InputPinLabel, const bool bTransactional, const bool bRequired)
	{
		if (const TSharedPtr<FPointIO> SingleIO = TryGetSingleInput(InContext, InputPinLabel, bTransactional, bRequired))
		{
			return MakeShared<FFacade>(SingleIO.ToSharedRef());
		}

		return nullptr;
	}

	bool TryGetFacades(FPCGExContext* InContext, const FName InputPinLabel, TArray<TSharedPtr<FFacade>>& OutFacades, const bool bRequired, const bool bIsTransactional)
	{
		TSharedPtr<FPointIOCollection> TargetsCollection = MakeShared<FPointIOCollection>(InContext, InputPinLabel, EIOInit::NoInit, bIsTransactional);
		if (TargetsCollection->IsEmpty())
		{
			if (bRequired) { PCGEX_LOG_MISSING_INPUT(InContext, FText::Format(FText::FromString(TEXT("Missing or zero-points '{0}' inputs")), FText::FromName(InputPinLabel))) }
			return false;
		}

		OutFacades.Reserve(OutFacades.Num() + TargetsCollection->Num());
		for (const TSharedPtr<FPointIO>& IO : TargetsCollection->Pairs) { OutFacades.Add(MakeShared<FFacade>(IO.ToSharedRef())); }

		return true;
	}


#pragma endregion
}
