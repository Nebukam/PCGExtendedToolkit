// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"

class FPCGExSearchOperation;

namespace PCGExMT
{
	class FTaskManager;
}

struct FPCGExNodeSelectionDetails;

namespace PCGExData
{
	class FFacade;
}

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExHeuristics
{
	class FHandler;
	class FLocalFeedbackHandler;
}

namespace PCGExPathfinding
{
	class FSearchAllocations;
	class FPathQuery;

	class PCGEXELEMENTSPATHFINDING_API FPlotQuery : public TSharedFromThis<FPlotQuery>
	{
		TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler> LocalFeedbackHandler;

	public:
		explicit FPlotQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, bool ClosedLoop, const int32 InQueryIndex);

		TSharedRef<PCGExClusters::FCluster> Cluster;
		bool bIsClosedLoop = false;
		TSharedPtr<PCGExData::FFacade> PlotFacade;
		const int32 QueryIndex = -1;

		TArray<TSharedPtr<FPathQuery>> SubQueries;

		using CompletionCallback = std::function<void(const TSharedPtr<FPlotQuery>&)>;
		CompletionCallback OnCompleteCallback;

		void BuildPlotQuery(const TSharedPtr<PCGExData::FFacade>& InPlot, const FPCGExNodeSelectionDetails& SeedSelectionDetails, const FPCGExNodeSelectionDetails& GoalSelectionDetails);

		void FindPaths(
			const TSharedPtr<PCGExMT::FTaskManager>& TaskManager,
			const TSharedPtr<FPCGExSearchOperation>& SearchOperation,
			const TSharedPtr<FSearchAllocations>& Allocations,
			const TSharedPtr<PCGExHeuristics::FHandler>& HeuristicsHandler);

		void Cleanup();
	};
}
