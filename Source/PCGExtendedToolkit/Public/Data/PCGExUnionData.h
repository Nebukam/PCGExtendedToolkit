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
		using InlineSparseAllocator = TSetAllocator<TSparseArrayAllocator<TInlineAllocator<8>, TInlineAllocator<8>>, TInlineAllocator<8>>;
		
		//int32 Index = 0;
		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> IOSet;
		TSet<FElement, DefaultKeyFuncs<FElement>, InlineSparseAllocator> Elements;

		FUnionData() = default;
		~FUnionData() = default;

		FORCEINLINE int32 Num() const { return Elements.Num(); }

		// Gather data into arrays and return the required iteration count
		int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TMap<uint32, int32>& SourcesIdx,
			const FConstPoint& Target,
			const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
			TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const;

		void Add_Unsafe(const PCGExData::FPoint& Point);
		void Add(const PCGExData::FPoint& Point);

		void Add_Unsafe(const int32 IOIndex, const TArray<int32>& PointIndices);
		void Add(const int32 IOIndex, const TArray<int32>& PointIndices);

		void Reset()
		{
			IOSet.Reset();
			Elements.Reset();
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

		void Append(const int32 Index, const PCGExData::FPoint& Point);
		bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

		FORCEINLINE TSharedPtr<FUnionData> Get(const int32 Index) const { return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr; }
	};

#pragma endregion
}
