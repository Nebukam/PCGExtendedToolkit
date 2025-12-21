// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExCluster
{
	class FNodeChain;
	class FCluster;
}

namespace PCGExGraph
{
	class FGraph;
}

namespace PCGExCluster
{
	namespace ChainHelpers
	{
		PCGEXGRAPHS_API void Dump(
			const TSharedRef<PCGExCluster::FNodeChain>& Chain,
			const TSharedRef<PCGExCluster::FCluster>& Cluster,
			const TSharedPtr<PCGExGraph::FGraph>& Graph,
			const bool bAddMetadata);

		PCGEXGRAPHS_API void DumpReduced(
			const TSharedRef<PCGExCluster::FNodeChain>& Chain,
			const TSharedRef<PCGExCluster::FCluster>& Cluster,
			const TSharedPtr<PCGExGraph::FGraph>& Graph,
			const bool bAddMetadata);
	}
}
