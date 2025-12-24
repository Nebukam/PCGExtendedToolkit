// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGExCommon.h"

namespace PCGExGraphs
{
	class FSubGraph;
	class FGraphBuilder;

	using FGraphCompilationEndCallback = std::function<void(const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)>;
	using FSubGraphPostProcessCallback = std::function<void(const TSharedRef<FSubGraph>& InSubGraph)>;

	namespace States
	{
		PCGEX_CTX_STATE(State_WritingClusters)
		PCGEX_CTX_STATE(State_ReadyToCompile)
		PCGEX_CTX_STATE(State_Compiling)

		PCGEX_CTX_STATE(State_Pathfinding)
		PCGEX_CTX_STATE(State_WaitingPathfinding)
	}
}
