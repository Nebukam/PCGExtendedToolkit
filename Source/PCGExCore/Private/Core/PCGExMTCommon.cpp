// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExMTCommon.h"
#include "Async/ParallelFor.h"

namespace PCGExMT
{
#pragma region FScope

	FScope::FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex)
		: Start(InStart), Count(InCount), End(InStart + InCount), LoopIndex(InLoopIndex)
	{
	}

	void FScope::GetIndices(TArray<int32>& OutIndices) const
	{
		OutIndices.SetNumUninitialized(Count);
		for (int i = 0; i < Count; i++) { OutIndices[i] = Start + i; }
	}

#pragma endregion

#pragma region Parallel Helpers

	void ParallelOrSequential(const int32 Num, const FLoopBody& Body, const int32 Threshold)
	{
		if (Num >= Threshold)
		{
			ParallelFor(Num, Body);
		}
		else
		{
			for (int32 i = 0; i < Num; ++i)
			{
				Body(i);
			}
		}
	}

	void Sequential(const int32 Num, const FLoopBody& Body)
	{
		for (int32 i = 0; i < Num; ++i)
		{
			Body(i);
		}
	}

#pragma endregion
}
