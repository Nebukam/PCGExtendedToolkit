// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExClusters
{
	class FNodeChain;
	class FCluster;
}

namespace PCGExGraphs
{
	class FGraph;
}

namespace PCGExClusters::ChainHelpers
{
	PCGEXFOUNDATIONS_API void Dump(
		const TSharedRef<FNodeChain>& Chain,
		const TSharedRef<FCluster>& Cluster,
		const TSharedPtr<PCGExGraphs::FGraph>& Graph,
		const bool bAddMetadata);

	PCGEXFOUNDATIONS_API void DumpReduced(
		const TSharedRef<FNodeChain>& Chain,
		const TSharedRef<FCluster>& Cluster,
		const TSharedPtr<PCGExGraphs::FGraph>& Graph,
		const bool bAddMetadata);
}
