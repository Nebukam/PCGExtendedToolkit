// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExPlotQuery.h"

#include "PCGExHeuristicsHandler.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"
#include "Core/PCGExPathQuery.h"
#include "Search/PCGExSearchOperation.h"

namespace PCGExPathfinding
{
	void FPlotQuery::BuildPlotQuery(const TSharedPtr<PCGExData::FFacade>& InPlot, const FPCGExNodeSelectionDetails& SeedSelectionDetails, const FPCGExNodeSelectionDetails& GoalSelectionDetails)
	{
		PlotFacade = InPlot;
		SubQueries.Reserve(InPlot->GetNum());

		TSharedPtr<FPathQuery> PrevQuery = MakeShared<FPathQuery>(Cluster, PlotFacade->Source->GetInPoint(0), PlotFacade->Source->GetInPoint(1), 0);

		PrevQuery->ResolvePicks(SeedSelectionDetails, GoalSelectionDetails);

		SubQueries.Add(PrevQuery);

		for (int i = 2; i < PlotFacade->GetNum(); i++)
		{
			TSharedPtr<FPathQuery> NextQuery = MakeShared<FPathQuery>(Cluster, PrevQuery, PlotFacade->Source->GetInPoint(i), i - 1);
			NextQuery->ResolvePicks(SeedSelectionDetails, GoalSelectionDetails);

			SubQueries.Add(NextQuery);
			PrevQuery = NextQuery;
		}

		if (bIsClosedLoop)
		{
			TSharedPtr<FPathQuery> WrapQuery = MakeShared<FPathQuery>(Cluster, SubQueries.Last(), SubQueries[0], PlotFacade->GetNum());
			WrapQuery->ResolvePicks(SeedSelectionDetails, GoalSelectionDetails);
			SubQueries.Add(WrapQuery);
		}
	}

	void FPlotQuery::FindPaths(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager, const TSharedPtr<FPCGExSearchOperation>& SearchOperation, const TSharedPtr<FSearchAllocations>& Allocations, const TSharedPtr<PCGExHeuristics::FHandler>& HeuristicsHandler)
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(TaskManager, PlotTasks)

		LocalFeedbackHandler = HeuristicsHandler->MakeLocalFeedbackHandler(Cluster);

		PlotTasks->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->LocalFeedbackHandler.Reset();
			if (This->OnCompleteCallback) { This->OnCompleteCallback(This); }
		};

		PlotTasks->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE, SearchOperation, Allocations, HeuristicsHandler](const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS
			TSharedPtr<FSearchAllocations> LocalAllocations = Allocations;
			if (!Allocations) { LocalAllocations = SearchOperation->NewAllocations(); }
			PCGEX_SCOPE_LOOP(Index)
			{
				This->SubQueries[Index]->FindPath(SearchOperation, LocalAllocations, HeuristicsHandler, This->LocalFeedbackHandler);
			}
		};

		PlotTasks->StartSubLoops(SubQueries.Num(), 12, HeuristicsHandler->HasAnyFeedback() || (Allocations != nullptr));
	}

	FPlotQuery::FPlotQuery(const TSharedRef<PCGExClusters::FCluster>& InCluster, bool ClosedLoop, const int32 InQueryIndex)
		: Cluster(InCluster), bIsClosedLoop(ClosedLoop), QueryIndex(InQueryIndex)
	{
	}

	void FPlotQuery::Cleanup()
	{
		for (const TSharedPtr<FPathQuery>& Query : SubQueries) { Query->Cleanup(); }
		SubQueries.Empty();
	}
}
