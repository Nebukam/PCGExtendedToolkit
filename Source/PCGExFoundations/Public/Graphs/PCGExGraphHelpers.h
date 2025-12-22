// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"

class UPCGMetadata;

namespace PCGExData
{
	class FPointIO;
}

namespace PCGExGraphs
{
	class FGraph;

	namespace Helpers
	{
		bool BuildIndexedEdges(const TSharedPtr<PCGExData::FPointIO>& EdgeIO, const TMap<uint32, int32>& EndpointsLookup, TArray<FEdge>& OutEdges, const bool bStopOnError = false);

		bool BuildEndpointsLookup(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TMap<uint32, int32>& OutIndices, TArray<int32>& OutAdjacency);
	}
}
