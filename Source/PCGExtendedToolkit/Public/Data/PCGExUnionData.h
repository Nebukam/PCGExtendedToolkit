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

	class PCGEXTENDEDTOOLKIT_API IUnionData : public TSharedFromThis<IUnionData>
	{
	protected:
		mutable FRWLock UnionLock;

	public:
		using InlineSparseAllocator = TSetAllocator<TSparseArrayAllocator<TInlineAllocator<8>, TInlineAllocator<8>>, TInlineAllocator<8>>;

		//int32 Index = 0;
		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> IOSet;
		TSet<FElement, DefaultKeyFuncs<FElement>, InlineSparseAllocator> Elements;

		IUnionData() = default;
		virtual ~IUnionData() = default;

		FORCEINLINE int32 Num() const { return Elements.Num(); }

		// Gather data into arrays and return the required iteration count
		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const FConstPoint& Target,
			const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
			TArray<FWeightedPoint>& OutWeightedPoints) const;

		void Add_Unsafe(const FPoint& Point);
		void Add(const FPoint& Point);

		void Add_Unsafe(const int32 IOIndex, const TArray<int32>& PointIndices);
		void Add(const int32 IOIndex, const TArray<int32>& PointIndices);

		virtual void Reset()
		{
			IOSet.Reset();
			Elements.Reset();
		}
	};

	class PCGEXTENDEDTOOLKIT_API FUnionDataWeighted : public IUnionData
	{
	public:
		TMap<FElement, double> Weights;

		FUnionDataWeighted() = default;
		virtual ~FUnionDataWeighted() override = default;

		// Gather data into arrays and return the required iteration count
		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const FConstPoint& Target,
			const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
			TArray<FWeightedPoint>& OutWeightedPoints) const override;

		void AddWeight_Unsafe(const FElement& Element, const double InWeight);
		void AddWeight(const FElement& Element, const double InWeight);

		virtual void Reset() override
		{
			IUnionData::Reset();
			Weights.Reset();
		}
	};

	class PCGEXTENDEDTOOLKIT_API FUnionMetadata : public TSharedFromThis<FUnionMetadata>
	{
	public:
		TArray<TSharedPtr<IUnionData>> Entries;
		bool bIsAbstract = false;

		FUnionMetadata() = default;
		~FUnionMetadata() = default;

		int32 Num() const { return Entries.Num(); }
		void SetNum(const int32 InNum);

		TSharedPtr<IUnionData> NewEntry_Unsafe(const FConstPoint& Point);
		TSharedPtr<IUnionData> NewEntryAt_Unsafe(const int32 ItemIndex);

		void Append(const int32 Index, const FPoint& Point);
		bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

		FORCEINLINE TSharedPtr<IUnionData> Get(const int32 Index) const { return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr; }
	};

#pragma endregion
}
