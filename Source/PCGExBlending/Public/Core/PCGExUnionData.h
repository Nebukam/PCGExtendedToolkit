// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "UObject/Object.h"
#include "Data/PCGExPointElements.h"

namespace PCGExMath
{
	class FDistances;
}

namespace PCGEx
{
	class FIndexLookup;
}

class UPCGBasePointData;

namespace PCGExDetails
{
	class FDistances;
}

namespace PCGExData
{
	struct FConstPoint;
	struct FWeightedPoint;
	struct FPoint;
	struct FElement;

#pragma region Compound

	class PCGEXBLENDING_API IUnionData : public TSharedFromThis<IUnionData>
	{
	protected:
		mutable FRWLock UnionLock;

	public:
		//int32 Index = 0;
		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> IOSet;
		TArray<FElement, TInlineAllocator<8>> Elements;

		IUnionData() = default;
		virtual ~IUnionData() = default;

		FORCEINLINE int32 Num() const { return Elements.Num(); }

		FORCEINLINE void Add_Unsafe(const FElement& Point)
		{
			IOSet.Add(Point.IO);
			Elements.Emplace(Point.Index == -1 ? 0 : Point.Index, Point.IO);
		}

		void Add(const FElement& Point);

		FORCEINLINE void Add_Unsafe(const int32 Index, const int32 IO)
		{
			IOSet.Add(IO);
			Elements.Emplace(Index == -1 ? 0 : Index, IO);
		}

		void Add(const int32 Index, const int32 IO);

		void Add_Unsafe(const int32 IOIndex, const TArray<int32>& PointIndices);
		void Add(const int32 IOIndex, const TArray<int32>& PointIndices);

		bool IsEmpty() const { return Elements.IsEmpty(); }

		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const FPoint& Target,
			const PCGExMath::FDistances* InDistanceDetails,
			TArray<FWeightedPoint>& OutWeightedPoints) const;

		virtual void Reserve(const int32 InSetReserve, const int32 InElementReserve);
		virtual void Reset();
	};

	class PCGEXBLENDING_API FUnionMetadata : public TSharedFromThis<FUnionMetadata>
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

		FORCEINLINE void Append_Unsafe(const int32 Index, const FPoint& Point) { Entries[Index]->Add_Unsafe(Point); }
		FORCEINLINE void Append(const int32 Index, const FPoint& Point) { Entries[Index]->Add(Point); }

		bool IOIndexOverlap(const int32 InIdx, const TSet<int32>& InIndices);

		FORCEINLINE TSharedPtr<IUnionData> Get(const int32 Index) const
		{
			return Entries.IsValidIndex(Index) ? Entries[Index] : nullptr;
		}
	};

#pragma endregion
}
