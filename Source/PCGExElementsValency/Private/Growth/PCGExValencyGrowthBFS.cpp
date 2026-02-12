// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Growth/PCGExValencyGrowthBFS.h"

#pragma region FPCGExValencyGrowthBFS

int32 FPCGExValencyGrowthBFS::SelectNextSocket(TArray<FPCGExOpenSocket>& Frontier)
{
	if (Frontier.IsEmpty()) { return INDEX_NONE; }

	// BFS: pop from front (FIFO)
	return 0;
}

#pragma endregion
