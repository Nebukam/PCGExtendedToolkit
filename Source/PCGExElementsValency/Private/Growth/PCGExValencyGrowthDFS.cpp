// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Growth/PCGExValencyGrowthDFS.h"

#pragma region FPCGExValencyGrowthDFS

int32 FPCGExValencyGrowthDFS::SelectNextConnector(TArray<FPCGExOpenConnector>& Frontier)
{
	if (Frontier.IsEmpty()) { return INDEX_NONE; }

	// DFS: pop from back (LIFO)
	return Frontier.Num() - 1;
}

#pragma endregion
