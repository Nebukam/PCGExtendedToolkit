// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGEx
{
	class FScoredQueue;
	class FHashLookup;
}

namespace PCGExPathfinding
{
	class PCGEXELEMENTSPATHFINDING_API FSearchAllocations : public TSharedFromThis<FSearchAllocations>
	{
	protected:
		int32 NumNodes = 0;

	public:
		FSearchAllocations() = default;

		TBitArray<> Visited;
		TArray<double> GScore;
		TSharedPtr<PCGEx::FHashLookup> TravelStack;
		TSharedPtr<PCGEx::FScoredQueue> ScoredQueue;

		void Init(const PCGExClusters::FCluster* InCluster);
		void Reset();
	};
}
