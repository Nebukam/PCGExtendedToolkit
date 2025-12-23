// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExMTCommon.h"

namespace PCGExMT
{
	FScope::FScope(const int32 InStart, const int32 InCount, const int32 InLoopIndex)
		: Start(InStart), Count(InCount), End(InStart + InCount), LoopIndex(InLoopIndex)
	{
	}

	void FScope::GetIndices(TArray<int32>& OutIndices) const
	{
		OutIndices.SetNumUninitialized(Count);
		for (int i = 0; i < Count; i++) { OutIndices[i] = Start + i; }
	}
}
