// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExH.h"
#include "CoreMinimal.h"

namespace PCGExSortingHelpers
{
	using PCGEx::FIndexKey;

	struct FVectorKey
	{
		FVectorKey() = default;

		FVectorKey(const int32 InIndex, const FVector& InVector)
			: X(InVector.X), Y(InVector.Y), Z(InVector.Z), Index(InIndex)
		{
		}

		double X;
		double Y;
		double Z;
		int32 Index;

		bool operator<(const FVectorKey& Other) const
		{
			if (X != Other.X) { return X < Other.X; }
			if (Y != Other.Y) { return Y < Other.Y; }
			return Z < Other.Z;
		}
	};

	static void RadixSort(TArray<FIndexKey>& Keys)
	{
		const int32 N = Keys.Num();
		if (N <= 1) { return; }

		// Radix sort by Key
		constexpr int32 NUM_BUCKETS = 256;
		constexpr int32 NUM_PASSES = sizeof(uint64);

		TArray<FIndexKey> Temp;
		Temp.SetNumUninitialized(N);

		FIndexKey* Curr = Keys.GetData();
		FIndexKey* Out = Temp.GetData();

		for (int32 pass = 0; pass < NUM_PASSES; ++pass)
		{
			int32 Count[NUM_BUCKETS] = {};
			int32 Shift = pass * 8;

			for (int32 i = 0; i < N; ++i)
			{
				Count[(Curr[i].Key >> Shift) & 0xFF]++;
			}

			int32 Sum[NUM_BUCKETS];
			int32 s = 0;
			for (int32 i = 0; i < NUM_BUCKETS; ++i)
			{
				Sum[i] = s;
				s += Count[i];
			}

			for (int32 i = 0; i < N; ++i)
			{
				Out[Sum[(Curr[i].Key >> Shift) & 0xFF]++] = Curr[i];
			}

			Swap(Curr, Out);
		}
	}
}
