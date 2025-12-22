// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExSearchAllocations.h"

#include "Containers/PCGExHashLookup.h"
#include "Utils/PCGExScoredQueue.h"

namespace PCGExPathfinding
{
	void FSearchAllocations::Reset()
	{
		if (GScore.Num() == Visited.Num())
		{
			for (int i = 0; i < NumNodes; i++)
			{
				Visited[i] = false;
				GScore[i] = -1;
			}
		}
		else
		{
			for (int i = 0; i < NumNodes; i++) { Visited[i] = false; }
		}

		TravelStack->Reset();
		ScoredQueue->Reset();
	}
}
