// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGEx.h"

#include "PCGExH.h"
#include "Metadata/PCGAttributePropertySelector.h"

namespace PCGEx
{
	

	void ScopeIndices(const TArray<int32>& InIndices, TArray<uint64>& OutScopes)
	{
		TArray<int32> InIndicesCopy = InIndices;
		InIndicesCopy.Sort();

		int32 StartIndex = InIndicesCopy[0];
		int32 LastIndex = StartIndex;
		int32 Count = 1;

		for (int i = 1; i < InIndicesCopy.Num(); i++)
		{
			const int32 NextIndex = InIndicesCopy[i];
			if (NextIndex == (LastIndex + 1))
			{
				Count++;
				LastIndex = NextIndex;
				continue;
			}

			OutScopes.Emplace(H64(StartIndex, Count));
			LastIndex = StartIndex = NextIndex;
			Count = 0;
		}

		OutScopes.Emplace(H64(StartIndex, Count));
	}
}
