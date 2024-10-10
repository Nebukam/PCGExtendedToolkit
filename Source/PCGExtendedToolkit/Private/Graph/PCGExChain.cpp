// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExChain.h"
#include "Graph/PCGExCluster.h"
#include "Geometry/PCGExGeo.h"

namespace PCGExCluster
{
}

namespace PCGExClusterTask
{
	bool FFindNodeChains::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FFindNodeChains::ExecuteTask);

		//TArray<uint64> AdjacencyHashes;
		//AdjacencyHashes.Reserve(Cluster->Nodes.Num());

		TArray<PCGExCluster::FNode>& NodesRefs = *Cluster->Nodes;
		TSet<int32> IgnoredEdges;
		IgnoredEdges.Reserve(Cluster->Nodes->Num());

		bool bIsAlreadyIgnored;
		int32 LastSimpleNodeIndex = -1;

		for (const PCGExCluster::FNode& Node : NodesRefs)
		{
			const TArray<bool>& Brkpts = *Breakpoints;
			if (Node.IsSimple())
			{
				// Keep earliest simple node index in case we find no good starting points
				LastSimpleNodeIndex = LastSimpleNodeIndex == -1 ? Node.NodeIndex : FMath::Min(LastSimpleNodeIndex, Node.NodeIndex);
				if (!Brkpts[Node.NodeIndex]) { continue; }
			}

			const bool bIsValidStartNode =
				bDeadEndsOnly ?
					Node.IsDeadEnd() && !Brkpts[Node.NodeIndex] :
					(Node.IsDeadEnd() || Brkpts[Node.NodeIndex] || Node.IsComplex());

			if (!bIsValidStartNode) { continue; }

			for (const uint64 AdjacencyHash : Node.Adjacency)
			{
				uint32 OtherNodeIndex;
				uint32 EdgeIndex;
				PCGEx::H64(AdjacencyHash, OtherNodeIndex, EdgeIndex);

				IgnoredEdges.Add(EdgeIndex, &bIsAlreadyIgnored);
				if (bIsAlreadyIgnored) { continue; }

				const PCGExCluster::FNode& OtherNode = NodesRefs[OtherNodeIndex];

				if (Brkpts[OtherNode.NodeIndex] || OtherNode.IsDeadEnd() || OtherNode.IsComplex())
				{
					// Single edge chain					
					if (bSkipSingleEdgeChains) { continue; }

					TSharedPtr<PCGExCluster::FNodeChain> NewChain = MakeShared<PCGExCluster::FNodeChain>();
					NewChain->First = Node.NodeIndex;
					NewChain->Last = OtherNodeIndex;
					NewChain->SingleEdge = EdgeIndex;

					Chains->Add(NewChain);
				}
				else
				{
					// Requires extended Search
					InternalStart<FBuildChain>(
						Chains->Add(nullptr), nullptr,
						Cluster, Breakpoints, Chains, Node.NodeIndex, AdjacencyHash);
				}
			}
		}

		if (Chains->IsEmpty() && LastSimpleNodeIndex != -1)
		{
			// Assume isolated closed loop, probably contour

			PCGExCluster::FNode& StartNode = NodesRefs[LastSimpleNodeIndex];

			if (StartNode.Adjacency.IsEmpty()) { return false; }

			InternalStart<FBuildChain>(
				Chains->Add(nullptr), nullptr,
				Cluster, Breakpoints, Chains, StartNode.NodeIndex, StartNode.Adjacency[0]);
		}

		return !Chains->IsEmpty();
	}

	bool FBuildChain::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBuildChain::ExecuteTask);

		const TSharedPtr<PCGExCluster::FNodeChain> NewChain = MakeShared<PCGExCluster::FNodeChain>();

		uint32 NodeIndex;
		uint32 EdgeIndex;
		PCGEx::H64(AdjacencyHash, NodeIndex, EdgeIndex);

		NewChain->First = StartIndex;
		NewChain->Last = NodeIndex;

		BuildChain(NewChain, Breakpoints, Cluster);
		(*Chains)[TaskIndex] = NewChain;

		return true;
	}
}
