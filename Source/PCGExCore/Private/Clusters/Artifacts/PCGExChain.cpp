// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExChain.h"

#include "Algo/RemoveIf.h"
#include "Data/PCGExPointIO.h"
#include "Clusters/PCGExCluster.h"

namespace PCGExClusters
{
	void FNodeChain::FixUniqueHash()
	{
		UniqueHash = 0;

		if (Links.Num() <= 1)
		{
			SingleEdge = Seed.Edge;
			UniqueHash = SingleEdge;
			return;
		}

		const FLink LastLink = Links.Last();
		UniqueHash = PCGEx::H64U(HashCombineFast(Seed.Node, Seed.Edge), HashCombineFast(LastLink.Node, LastLink.Edge));
	}

	void FNodeChain::BuildChain(const TSharedRef<FCluster>& Cluster, const TSharedPtr<TArray<int8>>& Breakpoints)
	{
		ON_SCOPE_EXIT
		{
			bIsLeaf = Cluster->GetNode(Seed.Node)->IsLeaf() || Cluster->GetNode(Links.Last().Node)->IsLeaf();
			if (bIsClosedLoop) { bIsLeaf = false; }
			FixUniqueHash();
		};

		TSet<int32> Visited;

		FLink Last = Seed;
		FNode* FromNode = Cluster->GetEdgeOtherNode(Seed);
		Links.Add(FLink(FromNode->Index, Seed.Edge));

		Visited.Add(Seed.Node);
		Visited.Add(Links.Last().Node);

		while (FromNode)
		{
			if (FromNode->IsLeaf() || FromNode->IsComplex() || (Breakpoints && (*Breakpoints)[FromNode->PointIndex]))
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

	FVector FNodeChain::GetFirstEdgeDir(const TSharedPtr<FCluster>& Cluster) const
	{
		return Cluster->GetDir(Seed.Node, (*Cluster->NodeIndexLookup)[Cluster->GetEdge(Seed.Edge)->Other(Cluster->GetNodePointIndex(Seed.Node))]);
	}

	FVector FNodeChain::GetLastEdgeDir(const TSharedPtr<FCluster>& Cluster) const
	{
		const FLink& Lk = Links.Last();
		return Cluster->GetDir(Lk.Node, (*Cluster->NodeIndexLookup)[Cluster->GetEdge(Lk.Edge)->Other(Cluster->GetNodePointIndex(Lk.Node))]);
	}

	FVector FNodeChain::GetEdgeDir(const TSharedPtr<FCluster>& Cluster, const bool bFirst) const
	{
		if (bFirst) { return GetFirstEdgeDir(Cluster); }
		return GetLastEdgeDir(Cluster);
	}

	int32 FNodeChain::GetNodes(const TSharedPtr<FCluster>& Cluster, TArray<int32>& OutNodes, const bool bReverse)
	{
		if (SingleEdge != -1)
		{
			OutNodes.Reset(2);
			FEdge* Edge = Cluster->GetEdge(SingleEdge);

			if (bReverse)
			{
				OutNodes.Add(Cluster->GetNode((*Cluster->NodeIndexLookup)[Edge->End])->Index);
				OutNodes.Add(Cluster->GetNode((*Cluster->NodeIndexLookup)[Edge->Start])->Index);
			}
			else
			{
				OutNodes.Add(Cluster->GetNode((*Cluster->NodeIndexLookup)[Edge->Start])->Index);
				OutNodes.Add(Cluster->GetNode((*Cluster->NodeIndexLookup)[Edge->End])->Index);
			}

			return 2;
		}

		const int32 ChainSize = Links.Num();

		OutNodes.Reset(ChainSize + 1);

		if (bReverse)
		{
			for (int i = ChainSize - 1; i >= 0; i--) { OutNodes.Add(Links[i].Node); }
			OutNodes.Add(Seed.Node);
		}
		else
		{
			OutNodes.Add(Seed.Node);
			for (int i = 0; i < ChainSize; i++) { OutNodes.Add(Links[i].Node); }
		}

		return OutNodes.Num();
	}

	bool FNodeChainBuilder::Compile(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
	{
		Chains.Reserve(Cluster->Edges->Num());
		int32 NumBinaries = 0;

		for (int i = 0; i < Cluster->Nodes->Num(); i++)
		{
			FNode* Node = Cluster->GetNode(i);
			ensure(!Node->IsEmpty());

			if (Node->IsEmpty()) { continue; }
			if (Node->IsLeaf())
			{
				PCGEX_MAKE_SHARED(NewChain, FNodeChain, FLink(Node->Index, Node->Links[0].Edge))
				Chains.Add(NewChain);
				continue;
			}

			if (Node->IsBinary())
			{
				if (!Breakpoints || !(*Breakpoints)[Node->PointIndex])
				{
					NumBinaries++;
					continue;
				}
			}

			for (const FLink& Lk : Node->Links)
			{
				// Skip immediately known leaves or already seeded nodes. Avoid double-sampling simple cases
				if (Cluster->GetNode(Lk.Node)->IsLeaf()) { continue; }

				PCGEX_MAKE_SHARED(NewChain, FNodeChain, FLink(Node->Index, Lk.Edge))
				Chains.Add(NewChain);
			}
		}

		Chains.Shrink();
		if (Chains.IsEmpty())
		{
			if (NumBinaries > 0 && NumBinaries == Cluster->Nodes->Num())
			{
				// That's an isolated closed loop
				PCGEX_MAKE_SHARED(NewChain, FNodeChain, Cluster->GetNode(0)->Links[0])
				Chains.Add(NewChain);
			}
			else
			{
				return false;
			}
		}
		return DispatchTasks(TaskManager);
	}

	bool FNodeChainBuilder::CompileLeavesOnly(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
	{
		Chains.Reserve(Cluster->Edges->Num());

		for (int i = 0; i < Cluster->Nodes->Num(); i++)
		{
			FNode* Node = Cluster->GetNode(i);
			ensure(!Node->IsEmpty());
			if (!Node->IsLeaf() || Node->IsEmpty()) { continue; }

			PCGEX_MAKE_SHARED(NewChain, FNodeChain, FLink(Node->Index, Node->Links[0].Edge))
			Chains.Add(NewChain);
		}

		Chains.Shrink();
		if (Chains.IsEmpty()) { return false; }
		return DispatchTasks(TaskManager);
	}

	bool FNodeChainBuilder::DispatchTasks(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
	{
		PCGEX_ASYNC_GROUP_CHKD(TaskManager, ChainSearchTask)

		ChainSearchTask->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			This->Dedupe();
		};

		ChainSearchTask->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS
			This->Chains[Index]->BuildChain(This->Cluster, This->Breakpoints);
		};

		ChainSearchTask->StartIterations(Chains.Num(), 64, false);
		return true;
	}

	FNodeChainBuilder::FNodeChainBuilder(const TSharedRef<FCluster>& InCluster)
		: Cluster(InCluster)
	{
	}

	void FNodeChainBuilder::Dedupe()
	{
		TSet<uint64> UniqueHashSet;
		UniqueHashSet.Reserve(Chains.Num());

		Chains.SetNum(Algo::StableRemoveIf(
			Chains,
			[&UniqueHashSet](const TSharedPtr<FNodeChain>& Chain)
			{
				bool bAlreadySet = false;
				UniqueHashSet.Add(Chain->UniqueHash, &bAlreadySet);
				return bAlreadySet; // remove duplicates
			}
		));
	}
}
