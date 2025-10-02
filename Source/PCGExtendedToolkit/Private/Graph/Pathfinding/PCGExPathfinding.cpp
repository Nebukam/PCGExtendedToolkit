﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Pathfinding/PCGExPathfinding.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Graph/PCGExCluster.h"
#include "Graph/Pathfinding/Heuristics/PCGExHeuristics.h"
#include "Graph/Pathfinding/Search/PCGExSearchOperation.h"

namespace PCGExPathfinding
{
	bool FNodePick::ResolveNode(const TSharedRef<PCGExCluster::FCluster>& InCluster, const FPCGExNodeSelectionDetails& SelectionDetails)
	{
		if (Node != nullptr) { return true; }

		const FVector SourcePosition = Point.GetLocation();
		const int32 NodeIndex = InCluster->FindClosestNode(SourcePosition, SelectionDetails.PickingMethod);
		if (NodeIndex == -1) { return false; }
		Node = InCluster->GetNode(NodeIndex);
		if (!SelectionDetails.WithinDistance(SourcePosition, InCluster->GetPos(Node)))
		{
			Node = nullptr;
			return false;
		}
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
		const TSharedPtr<FPCGExSearchOperation>& SearchOperation,
		const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& HeuristicsHandler,
		const TSharedPtr<PCGExHeuristics::FLocalFeedbackHandler>& LocalFeedback)
	{
		if (PickResolution != EQueryPickResolution::Success)
		{
			SetResolution(EPathfindingResolution::Fail);
			return;
		}

		PCGEX_SHARED_THIS_DECL

		if (SearchOperation->ResolveQuery(ThisPtr, HeuristicsHandler, LocalFeedback))
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
		TArray<int32>& OutPoints,
		const int32 TruncateStart,
		const int32 TruncateEnd) const
	{
		const int32 Count = PathNodes.Num() - TruncateEnd;
		for (int i = TruncateStart; i < Count; i++) { OutPoints.Add(Cluster->GetNodePointIndex(PathNodes[i])); }
	}

	void FPathQuery::AppendEdgePoints(TArray<int32>& OutPoints) const
	{
		OutPoints.Reserve(OutPoints.Num() + PathEdges.Num());
		OutPoints.Append(PathEdges);
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
			PlotFacade->Source->GetInPoint(0),
			PlotFacade->Source->GetInPoint(1),
			0);

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

	void FPlotQuery::FindPaths(
		const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager,
		const TSharedPtr<FPCGExSearchOperation>& SearchOperation,
		const TSharedPtr<PCGExHeuristics::FHeuristicsHandler>& HeuristicsHandler)
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, PlotTasks)

		LocalFeedbackHandler = HeuristicsHandler->MakeLocalFeedbackHandler(Cluster);

		PlotTasks->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->LocalFeedbackHandler.Reset();
				if (This->OnCompleteCallback) { This->OnCompleteCallback(This); }
			};

		PlotTasks->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE, SearchOperation, HeuristicsHandler](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->SubQueries[Scope.Start]->FindPath(SearchOperation, HeuristicsHandler, This->LocalFeedbackHandler);
			};

		PlotTasks->StartSubLoops(SubQueries.Num(), 1, HeuristicsHandler->HasAnyFeedback());
	}

	void FPlotQuery::Cleanup()
	{
		for (const TSharedPtr<FPathQuery>& Query : SubQueries) { Query->Cleanup(); }
		SubQueries.Empty();
	}

	void ProcessGoals(const TSharedPtr<PCGExData::FFacade>& InSeedDataFacade, const UPCGExGoalPicker* GoalPicker, TFunction<void(int32, int32)>&& GoalFunc)
	{
		for (int PointIndex = 0; PointIndex < InSeedDataFacade->Source->GetNum(); PointIndex++)
		{
			const PCGExData::FConstPoint& Seed = InSeedDataFacade->GetInPoint(PointIndex);

			if (GoalPicker->OutputMultipleGoals())
			{
				TArray<int32> GoalIndices;
				GoalPicker->GetGoalIndices(Seed, GoalIndices);
				for (const int32 GoalIndex : GoalIndices)
				{
					if (GoalIndex < 0) { continue; }
					GoalFunc(PointIndex, GoalIndex);
				}
			}
			else
			{
				const int32 GoalIndex = GoalPicker->GetGoalIndex(Seed);
				if (GoalIndex < 0) { continue; }
				GoalFunc(PointIndex, GoalIndex);
			}
		}
	}
}
