// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Growth/PCGExValencyGrowthRandom.h"

#pragma region FPCGExValencyGrowthRandom

int32 FPCGExValencyGrowthRandom::SelectNextSocket(TArray<FPCGExOpenSocket>& Frontier)
{
	if (Frontier.IsEmpty()) { return INDEX_NONE; }

	// Random: pick any socket
	return RandomStream.RandRange(0, Frontier.Num() - 1);
}

#pragma endregion
