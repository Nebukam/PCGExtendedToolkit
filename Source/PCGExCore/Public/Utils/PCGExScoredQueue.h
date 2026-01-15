// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <queue>
#include <vector>
#include "CoreMinimal.h"

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGEx
{
	class FScoredQueue
	{
	protected:
		// Heap storage: pairs of (score, nodeIndex)
		TArray<TPair<double, int32>> Heap;

		// Maps node index -> position in heap (-1 if not in queue)
		TArray<int32> HeapIndex;

		int32 Size = 0;

		FORCEINLINE int32 Parent(const int32 i) const { return (i - 1) >> 1; }
		FORCEINLINE int32 LeftChild(const int32 i) const { return (i << 1) + 1; }
		FORCEINLINE int32 RightChild(const int32 i) const { return (i << 1) + 2; }

		FORCEINLINE void Swap(const int32 i, const int32 j)
		{
			HeapIndex[Heap[i].Value] = j;
			HeapIndex[Heap[j].Value] = i;
			::Swap(Heap[i], Heap[j]);
		}

		void SiftUp(int32 i)
		{
			while (i > 0)
			{
				const int32 p = Parent(i);
				if (Heap[i].Key >= Heap[p].Key) { break; }
				Swap(i, p);
				i = p;
			}
		}

		void SiftDown(int32 i)
		{
			while (true)
			{
				int32 Smallest = i;
				const int32 L = LeftChild(i);
				const int32 R = RightChild(i);

				if (L < Size && Heap[L].Key < Heap[Smallest].Key) { Smallest = L; }
				if (R < Size && Heap[R].Key < Heap[Smallest].Key) { Smallest = R; }

				if (Smallest == i) { break; }
				Swap(i, Smallest);
				i = Smallest;
			}
		}

	public:
		TArray<double> Scores; // Public for compatibility with existing code

		explicit FScoredQueue(const int32 InSize)
		{
			Heap.Reserve(InSize);
			HeapIndex.Init(-1, InSize);
			Scores.Init(MAX_dbl, InSize);
		}

		FORCEINLINE bool IsEmpty() const { return Size == 0; }
		FORCEINLINE int32 Num() const { return Size; }

		bool Enqueue(const int32 Index, const double InScore)
		{
			double& RegisteredScore = Scores[Index];
			if (RegisteredScore <= InScore) { return false; }

			RegisteredScore = InScore;

			const int32 ExistingPos = HeapIndex[Index];
			if (ExistingPos != -1)
			{
				// Decrease-key: update existing entry
				Heap[ExistingPos].Key = InScore;
				SiftUp(ExistingPos); // Score only decreases, so only sift up
			}
			else
			{
				// Insert new entry
				const int32 Pos = Size++;
				if (Pos < Heap.Num()) { Heap[Pos] = TPair<double, int32>(InScore, Index); }
				else { Heap.Emplace(InScore, Index); }
				HeapIndex[Index] = Pos;
				SiftUp(Pos);
			}

			return true;
		}

		bool Dequeue(int32& OutItem, double& OutScore)
		{
			if (Size == 0) { return false; }

			OutItem = Heap[0].Value;
			OutScore = Heap[0].Key;
			HeapIndex[OutItem] = -1;

			Size--;
			if (Size > 0)
			{
				Heap[0] = Heap[Size];
				HeapIndex[Heap[0].Value] = 0;
				SiftDown(0);
			}

			return true;
		}

		void Reset()
		{
			for (int32 i = 0; i < Size; i++) { HeapIndex[Heap[i].Value] = -1; }
			Size = 0;
			for (double& Score : Scores) { Score = MAX_dbl; }
		}
	};
}
