// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExChain.h"
#include "Graph/PCGExCluster.h"

namespace PCGExCluster
{
	void FNodeChain::FixUniqueHash()
	{
		UniqueHash = 0;
		SingleEdge = -1;

		if (Links.Num() <= 1) { SingleEdge = Seed.Edge; }

		if (SingleEdge != -1) { UniqueHash = PCGEx::H64U(SingleEdge, SingleEdge); }
		else { UniqueHash = PCGEx::H64U(bIsClosedLoop ? Seed.Edge : Links[0].Edge, Links.Last().Edge); }
	}

	void FNodeChain::BuildChain(const TSharedRef<FCluster>& Cluster, const TSharedPtr<TArray<int8>>& Breakpoints)
	{
		ON_SCOPE_EXIT
		{
			FixUniqueHash();
			bIsLeaf = Cluster->GetNode(Seed.Node)->IsLeaf() || Cluster->GetNode(Links.Last().Node)->IsLeaf();
			if (bIsClosedLoop) { bIsLeaf = false; }
		};

		TSet<int32> Visited;

		FLink Last = Seed;
		FNode* FromNode = Cluster->GetEdgeOtherNode(Seed);
		Links.Add(FLink(FromNode->Index, Seed.Edge));

		Visited.Add(Seed.Node);
		Visited.Add(Links.Last().Node);

		while (FromNode)
		{
			if (FromNode->IsLeaf() ||
				FromNode->IsComplex() ||
				(Breakpoints && (*Breakpoints)[FromNode->PointIndex]))
			{
				bIsClosedLoop = false;
				break;
			}

			FLink NextLink = FromNode->Links[0];                               // Get next node
			if (NextLink.Node == Last.Node) { NextLink = FromNode->Links[1]; } // Get other next

			bool bAlreadyVisited = false;
			Visited.Add(NextLink.Node, &bAlreadyVisited);

			if (bAlreadyVisited || NextLink.Node == Seed.Node)
			{
				Seed.Edge = NextLink.Edge; // !
				bIsClosedLoop = true;
				break;
			}

			Last = Links.Last();
			Links.Add(NextLink);

			FromNode = Cluster->GetNode(NextLink.Node);
		}
	}

	void FNodeChain::Dump(const TSharedRef<FCluster>& Cluster, const TSharedPtr<PCGExGraph::FGraph>& Graph, const bool bAddMetadata) const
	{
		const int32 IOIndex = Cluster->EdgesIO.Pin()->IOIndex;
		FEdge OutEdge = FEdge{};

		if (SingleEdge != -1)
		{
			Graph->InsertEdge(*Cluster->GetEdge(Seed.Edge), OutEdge, IOIndex);
			if (bAddMetadata) { Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1; }
		}
		else
		{
			if (bAddMetadata)
			{
				if (bIsClosedLoop)
				{
					Graph->InsertEdge(*Cluster->GetEdge(Seed.Edge), OutEdge, IOIndex);
					Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1;
				}

				for (const FLink& Link : Links)
				{
					Graph->InsertEdge(*Cluster->GetEdge(Link.Edge), OutEdge, IOIndex);
					Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1;
				}
			}
			else
			{
				if (bIsClosedLoop) { Graph->InsertEdge(*Cluster->GetEdge(Seed.Edge), OutEdge, IOIndex); }
				for (const FLink& Link : Links) { Graph->InsertEdge(*Cluster->GetEdge(Link.Edge), OutEdge, IOIndex); }
			}
		}
	}

	void FNodeChain::DumpReduced(const TSharedRef<FCluster>& Cluster, const TSharedPtr<PCGExGraph::FGraph>& Graph, const bool bAddMetadata) const
	{
		const int32 IOIndex = Cluster->EdgesIO.Pin()->IOIndex;
		FEdge OutEdge = FEdge{};

		if (SingleEdge != -1)
		{
			Graph->InsertEdge(*Cluster->GetEdge(SingleEdge), OutEdge, IOIndex);
			if (bAddMetadata) { Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = 1; }
		}
		else
		{
			if (bAddMetadata)
			{
				Graph->InsertEdge(
					Cluster->GetNode(Seed)->PointIndex,
					Cluster->GetNode(Links.Last())->PointIndex,
					OutEdge, IOIndex);

				Graph->GetOrCreateEdgeMetadata(OutEdge.Index).UnionSize = Links.Num();
			}
			else
			{
				Graph->InsertEdge(
					Cluster->GetNode(Seed)->PointIndex,
					Cluster->GetNode(Links.Last())->PointIndex,
					OutEdge, IOIndex);
			}
		}
	}

	bool FNodeChainBuilder::Compile(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		Chains.Reserve(Cluster->Edges->Num());

		for (int i = 0; i < Cluster->Nodes->Num(); i++)
		{
			FNode* Node = Cluster->GetNode(i);
			ensure(!Node->IsEmpty());

			if (Node->IsEmpty()) { continue; }
			if (Node->IsLeaf())
			{
				TSharedPtr<FNodeChain> NewChain = MakeShared<FNodeChain>(FLink(Node->Index, Node->Links[0].Edge));
				Chains.Add(NewChain);
				continue;
			}

			if (Node->IsBinary()) { continue; }
			if (Breakpoints && !(*Breakpoints)[Node->PointIndex])

			{
				for (const FLink& Lk : Node->Links)
				{
					// Skip immediately known leaves or already seeded nodes. Avoid double-sampling simple cases
					if (Cluster->GetNode(Lk.Node)->IsLeaf()) { continue; }

					TSharedPtr<FNodeChain> NewChain = MakeShared<FNodeChain>(FLink(Node->Index, Lk.Edge));
					Chains.Add(NewChain);
				}
			}
		}

		Chains.Shrink();
		if (Chains.IsEmpty()) { return false; }
		return DispatchTasks(AsyncManager);
	}

	bool FNodeChainBuilder::CompileLeavesOnly(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		Chains.Reserve(Cluster->Edges->Num());

		for (int i = 0; i < Cluster->Nodes->Num(); i++)
		{
			FNode* Node = Cluster->GetNode(i);
			ensure(!Node->IsEmpty());
			if (!Node->IsLeaf() || Node->IsEmpty()) { continue; }

			TSharedPtr<FNodeChain> NewChain = MakeShared<FNodeChain>(FLink(Node->Index, Node->Links[0].Edge));
			Chains.Add(NewChain);
		}

		Chains.Shrink();
		if (Chains.IsEmpty()) { return false; }
		return DispatchTasks(AsyncManager);
	}

	bool FNodeChainBuilder::DispatchTasks(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, ChainSearchTask)

		ChainSearchTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->Dedupe();
			};

		ChainSearchTask->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]
			(const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->Chains[Index]->BuildChain(This->Cluster, This->Breakpoints);
			};

		ChainSearchTask->StartIterations(Chains.Num(), 64, false, false);
		return true;
	}

	void FNodeChainBuilder::Dedupe()
	{
		TSet<uint64> UniqueHashSet;
		UniqueHashSet.Reserve(Chains.Num());

		for (int i = 0; i < Chains.Num(); i++)
		{
			const TSharedPtr<FNodeChain>& Chain = Chains[i];

			bool bAlreadySet = false;
			UniqueHashSet.Add(Chain->UniqueHash, &bAlreadySet);

			if (bAlreadySet) { Chains[i] = nullptr; }
		}
	}
}
