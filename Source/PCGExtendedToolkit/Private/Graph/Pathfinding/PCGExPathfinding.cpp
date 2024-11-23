// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfinding.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

namespace PCGExPathfinding
{
	bool FNodePick::ResolveNode(const TSharedRef<PCGExCluster::FCluster>& InCluster, const FPCGExNodeSelectionDetails& SelectionDetails)
	{
		if (Node != nullptr) { return true; }

		const int32 NodeIndex = InCluster->FindClosestNode(SourcePosition, SelectionDetails.PickingMethod);
		if (NodeIndex == -1) { return false; }
		Node = InCluster->GetNode(NodeIndex);

		return true;
	}

	EQueryPickResolution FPathQuery::ResolvePicks(const FPCGExNodeSelectionDetails& SeedSelectionDetails, const FPCGExNodeSelectionDetails& GoalSelectionDetails)
	{
		PickResolution = EQueryPickResolution::None;

		if (!Seed.ResolveNode(Cluster, SeedSelectionDetails))
		{
			PickResolution = EQueryPickResolution::UnresolvedSeed;
		}

		if (!Goal.ResolveNode(Cluster, GoalSelectionDetails))
		{
			PickResolution = PickResolution == EQueryPickResolution::UnresolvedSeed ?
				                 EQueryPickResolution::UnresolvedPicks :
				                 EQueryPickResolution::UnresolvedGoal;
		}

		if (Seed.Node == Goal.Node && (PickResolution != EQueryPickResolution::UnresolvedSeed || PickResolution != EQueryPickResolution::UnresolvedGoal))
		{
			PickResolution = EQueryPickResolution::SameSeedAndGoal;
		}

		if (PickResolution == EQueryPickResolution::None) { PickResolution = EQueryPickResolution::Success; }

		return PickResolution;
	}

	void FPathQuery::Reserve(const int32 NumReserve)
	{
		PathNodes.Reserve(NumReserve);
		PathEdges.Reserve(NumReserve - 1);
	}

	void FPathQuery::AddPathNode(const int32 InNodeIndex, const int32 InEdgeIndex)
	{
		PathNodes.Add(InNodeIndex);
		if (InEdgeIndex != -1) { PathEdges.Add(InEdgeIndex); }
	}

	void FPathQuery::SetResolution(const EPathfindingResolution InResolution)
	{
		Resolution = InResolution;

		if (Resolution == EPathfindingResolution::Success)
		{
			Algo::Reverse(PathNodes);
			Algo::Reverse(PathEdges);
		}
	}

	void FPathQuery::FindPath(
		const UPCGExSearchOperation* SearchOperation,
		const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& HeuristicsHandler,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback)
	{
		if (PickResolution != EQueryPickResolution::Success)
		{
			SetResolution(EPathfindingResolution::Fail);
			return;
		}

		if (SearchOperation->ResolveQuery(SharedThis(this), HeuristicsHandler, LocalFeedback))
		{
			SetResolution(HasValidPathPoints() ? EPathfindingResolution::Success : EPathfindingResolution::Fail);
		}
		else
		{
			SetResolution(EPathfindingResolution::Fail);
		}

		if (Resolution == EPathfindingResolution::Fail) { return; }

		const TArray<PCGExCluster::FNode>& NodesRef = *Cluster->Nodes;
		const TArray<PCGExGraph::FEdge>& EdgesRef = *Cluster->Edges;

		// Feedback scores

		if (!HeuristicsHandler->HasAnyFeedback()) { return; }

		if (HeuristicsHandler->HasGlobalFeedback() && LocalFeedback)
		{
			for (int i = 0; i < PathEdges.Num(); i++)
			{
				const PCGExCluster::FNode& Node = NodesRef[PathNodes[i]];
				const PCGExGraph::FEdge& Edge = EdgesRef[PathEdges[i]];
				HeuristicsHandler->FeedbackScore(Node, Edge);
				LocalFeedback->FeedbackScore(Node, Edge);
			}

			HeuristicsHandler->FeedbackPointScore(NodesRef[PathNodes.Last()]);
			LocalFeedback->FeedbackPointScore(NodesRef[PathNodes.Last()]);
		}
		else if (LocalFeedback)
		{
			for (int i = 0; i < PathEdges.Num(); i++) { LocalFeedback->FeedbackScore(NodesRef[PathNodes[i]], EdgesRef[PathEdges[i]]); }
			LocalFeedback->FeedbackPointScore(NodesRef[PathNodes.Last()]);
		}
		else
		{
			for (int i = 0; i < PathEdges.Num(); i++) { HeuristicsHandler->FeedbackScore(NodesRef[PathNodes[i]], EdgesRef[PathEdges[i]]); }
			HeuristicsHandler->FeedbackPointScore(NodesRef[PathNodes.Last()]);
		}
	}

	void FPathQuery::AppendNodePoints(
		TArray<FPCGPoint>& OutPoints,
		const int32 TruncateStart,
		const int32 TruncateEnd) const
	{
		const int32 Count = PathNodes.Num() - TruncateEnd;
		for (int i = TruncateStart; i < Count; i++) { OutPoints.Add(*Cluster->GetNodePoint(PathNodes[i])); }
	}

	void FPathQuery::AppendEdgePoints(TArray<FPCGPoint>& OutPoints) const
	{
		const int32 Count = PathEdges.Num();
		TSharedPtr<PCGExData::FPointIO> EdgesIO = Cluster->EdgesIO.Pin();
		for (int i = 0; i < Count; i++) { OutPoints.Add(EdgesIO->GetInPoint(PathEdges[i])); }
	}

	void FPathQuery::Cleanup()
	{
		PathNodes.Empty();
		PathEdges.Empty();
	}

	void FPlotQuery::BuildPlotQuery(
		const TSharedPtr<PCGExData::FFacade>& InPlot,
		const FPCGExNodeSelectionDetails& SeedSelectionDetails,
		const FPCGExNodeSelectionDetails& GoalSelectionDetails)
	{
		PlotFacade = InPlot;
		SubQueries.Reserve(InPlot->GetNum());

		TSharedPtr<FPathQuery> PrevQuery = MakeShared<FPathQuery>(
			Cluster,
			PlotFacade->Source->GetInPointRef(0),
			PlotFacade->Source->GetInPointRef(1));

		PrevQuery->ResolvePicks(SeedSelectionDetails, GoalSelectionDetails);

		SubQueries.Add(PrevQuery);

		for (int i = 2; i < PlotFacade->GetNum(); i++)
		{
			TSharedPtr<FPathQuery> NextQuery = MakeShared<FPathQuery>(Cluster, PrevQuery, PlotFacade->Source->GetInPointRef(i));
			NextQuery->ResolvePicks(SeedSelectionDetails, GoalSelectionDetails);

			SubQueries.Add(NextQuery);
			PrevQuery = NextQuery;
		}

		if (bIsClosedLoop)
		{
			TSharedPtr<FPathQuery> WrapQuery = MakeShared<FPathQuery>(Cluster, SubQueries.Last(), SubQueries[0]);
			WrapQuery->ResolvePicks(SeedSelectionDetails, GoalSelectionDetails);
			SubQueries.Add(WrapQuery);
		}
	}

	void FPlotQuery::FindPaths(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const UPCGExSearchOperation* SearchOperation,
		const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& HeuristicsHandler)
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PlotTasks)

		LocalFeedbackHandler = HeuristicsHandler->MakeLocalFeedbackHandler(Cluster);

		PlotTasks->OnCompleteCallback =
			[WeakThis = TWeakPtr<FPlotQuery>(SharedThis(this))]()
			{
				if (const TSharedPtr<FPlotQuery> This = WeakThis.Pin())
				{
					This->LocalFeedbackHandler.Reset();
					if (This->OnCompleteCallback) { This->OnCompleteCallback(This); }
				}
			};

		PlotTasks->OnSubLoopStartCallback =
			[WeakThis = TWeakPtr<FPlotQuery>(SharedThis(this)), SearchOperation, HeuristicsHandler]
			(const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				TSharedPtr<FPlotQuery> This = WeakThis.Pin();
				if (!This) { return; }

				This->SubQueries[StartIndex]->FindPath(SearchOperation, HeuristicsHandler, This->LocalFeedbackHandler);
			};

		PlotTasks->StartSubLoops(SubQueries.Num(), 1, HeuristicsHandler->HasAnyFeedback());
	}

	void FPlotQuery::Cleanup()
	{
		for (const TSharedPtr<FPathQuery>& Query : SubQueries) { Query->Cleanup(); }
		SubQueries.Empty();
	}
}
