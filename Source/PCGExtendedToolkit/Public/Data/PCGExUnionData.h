// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExPointIO.h"
#include "PCGExDetailsData.h"

namespace PCGExData
{

#pragma region Compound

	class PCGEXTENDEDTOOLKIT_API FUnionData : public TSharedFromThis<FUnionData>
	{
	protected:
		mutable FRWLock UnionLock;

	public:
		//int32 Index = 0;
		TSet<int32> IOIndices;
		TSet<uint64> ItemHashSet;

		FUnionData() = default;
		~FUnionData() = default;

		int32 Num() const { return ItemHashSet.Num(); }

		void ComputeWeights(
			const TArray<TSharedPtr<FFacade>>& Sources,
			const TMap<uint32, int32>& SourcesIdx,
			const FPCGPoint& Target,
			const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
			TArray<int32>& OutIOIdx,
			TArray<int32>& OutPointsIdx,
			TArray<double>& OutWeights) const;

		uint64 Add(const PCGExData::FPoint& Point);
		void Add(const int32 IOIndex, const TArray<int32>& PointIndices);

		void Reset()
		{
			IOIndices.Reset();
			ItemHashSet.Reset();
		}
	};

	class PCGEXTENDEDTOOLKIT_API FUnionMetadata : public TSharedFromThis<FUnionMetadata>
	{
	public:
		TArray<TSharedPtr<FUnionData>> Entries;
		bool bIsAbstract = false;

		FUnionMetadata() = default;
		~FUnionMetadata() = default;

		int32 Num() const { return Entries.Num(); }
		void SetNum(const int32 InNum);

		TSharedPtr<FUnionData> NewEntry_Unsafe(const PCGExData::FConstPoint& Point);
		TSharedPtr<FUnionData> NewEntryAt_Unsafe(const int32 ItemIndex);

		uint64 Append(const int32 Index, const PCGExData::FPoint& Point);
		bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

		FORCEINLINE TSharedPtr<FUnionData> Get(const int32 Index) const { return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr; }
	};

#pragma endregion

}
