﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "UObject/Object.h"
#include "Data/PCGExPointElements.h"

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

	class PCGEXTENDEDTOOLKIT_API IUnionData : public TSharedFromThis<IUnionData>
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

		// Gather data into arrays and return the required iteration count
		virtual int32 ComputeWeights(
			const TArray<const UPCGBasePointData*>& Sources,
			const TSharedPtr<PCGEx::FIndexLookup>& IdxLookup,
			const FPoint& Target,
			const TSharedPtr<PCGExDetails::FDistances>& InDistanceDetails,
			TArray<FWeightedPoint>& OutWeightedPoints) const;

		void Add_Unsafe(const FElement& Point);
		void Add(const FElement& Point);

		void Add_Unsafe(const int32 IOIndex, const TArray<int32>& PointIndices);
		void Add(const int32 IOIndex, const TArray<int32>& PointIndices);

		bool IsEmpty() const { return Elements.IsEmpty(); }

		virtual void Reset()
		{
			IOSet.Reset();
			Elements.Reset();
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
