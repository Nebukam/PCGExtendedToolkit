// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExGraph.h"
#include "Geometry/PCGExGeo.h"

#include "PCGExCluster.h"

namespace PCGExCluster
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FNodeChain : public TSharedFromThis<FNodeChain>
	{
	public:
		
		bool bClosedLoop = false;
		int32 First = -1;
		int32 Last = -1;
		int32 SingleEdge = -1;
		TArray<int32> Nodes;
		TArray<int32> Edges;

		FNodeChain()
		{
		}

		~FNodeChain() = default;

		uint64 GetUniqueHash() const
		{
			if (SingleEdge != -1) { return SingleEdge; }

			if (First > Last)
			{
				if (Nodes.Num() == 1) { return HashCombineFast(HashCombineFast(Last, First), Nodes[0]); }
				return HashCombineFast(HashCombineFast(Last, First), HashCombineFast(Nodes.Last(), Nodes[0]));
			}

			if (Nodes.Num() == 1) { return HashCombineFast(HashCombineFast(First, Last), Nodes[0]); }
			return HashCombineFast(HashCombineFast(First, Last), HashCombineFast(Nodes[0], Nodes.Last()));

			/*
			const uint32 BaseHash = First > Last ? HashCombineFast(GetTypeHash(Last), GetTypeHash(First)) : HashCombineFast(GetTypeHash(First), GetTypeHash(Last));
			if (SingleEdge != -1) { return BaseHash; }
			if (Nodes.Num() == 1) { return HashCombineFast(BaseHash, GetTypeHash(Nodes[0])); }
			const uint32 NodeBaseHash = Nodes[0] > Nodes[1] ? HashCombineFast(GetTypeHash(Nodes[1]), GetTypeHash(Nodes[0])) : HashCombineFast(GetTypeHash(Nodes[0]), GetTypeHash(Nodes[1]));
			return HashCombineFast(BaseHash, NodeBaseHash);
			*/
		}
	};
}

namespace PCGExClusterTask
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FFindNodeChains final : public PCGExMT::FPCGExTask
	{
	public:
		FFindNodeChains(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		                const TSharedPtr<PCGExCluster::FCluster>& InCluster,
		                const TArray<int8>* InBreakpoints,
		                TArray<TSharedPtr<PCGExCluster::FNodeChain>>* InChains,
		                const bool InSkipSingleEdgeChains = false,
		                const bool InDeadEndsOnly = false) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			Breakpoints(InBreakpoints),
			Chains(InChains),
			bSkipSingleEdgeChains(InSkipSingleEdgeChains),
			bDeadEndsOnly(InDeadEndsOnly)
		{
		}

		TSharedPtr<PCGExCluster::FCluster> Cluster;
		const TArray<int8>* Breakpoints = nullptr;
		TArray<TSharedPtr<PCGExCluster::FNodeChain>>* Chains = nullptr;

		const bool bSkipSingleEdgeChains = false;
		const bool bDeadEndsOnly = false;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FBuildChain final : public PCGExMT::FPCGExTask
	{
	public:
		FBuildChain(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		            const TSharedPtr<const PCGExCluster::FCluster>& InCluster,
		            const TArray<int8>* InBreakpoints,
		            TArray<TSharedPtr<PCGExCluster::FNodeChain>>* InChains,
		            const int32 InStartIndex,
		            const uint64 InAdjacencyHash) :
			FPCGExTask(InPointIO),
			Cluster(InCluster),
			Breakpoints(InBreakpoints),
			Chains(InChains),
			StartIndex(InStartIndex),
			AdjacencyHash(InAdjacencyHash)
		{
		}

		TSharedPtr<const PCGExCluster::FCluster> Cluster;
		const TArray<int8>* Breakpoints = nullptr;
		TArray<TSharedPtr<PCGExCluster::FNodeChain>>* Chains = nullptr;
		int32 StartIndex = 0;
		uint64 AdjacencyHash = 0;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};

	static void BuildChain(
		const TSharedPtr<PCGExCluster::FNodeChain>& Chain,
		const TArray<int8>* Breakpoints,
		const TSharedPtr<const PCGExCluster::FCluster>& Cluster)
	{
		TArray<PCGExCluster::FNode>& Nodes = *Cluster->Nodes;

		const TArray<int8>& Brkpts = *Breakpoints;
		int32 LastIndex = Chain->First;
		int32 NextIndex = Chain->Last;
		Chain->Edges.Add(Nodes[LastIndex].GetEdgeIndex(NextIndex));

		while (NextIndex != -1)
		{
			const PCGExCluster::FNode& NextNode = Nodes[NextIndex];
			if (Brkpts[NextIndex] || NextNode.IsComplex() || NextNode.IsDeadEnd())
			{
				LastIndex = NextIndex;
				break;
			}

			uint32 OtherIndex;
			uint32 EdgeIndex;
			PCGEx::H64(NextNode.Adjacency[0], OtherIndex, EdgeIndex);                                  // Get next node
			if (OtherIndex == LastIndex) { PCGEx::H64(NextNode.Adjacency[1], OtherIndex, EdgeIndex); } // Get other next

			LastIndex = NextIndex;
			NextIndex = OtherIndex;

			Chain->Nodes.Add(LastIndex);
			Chain->Edges.Add(EdgeIndex);

			if (NextIndex == Chain->First)
			{
				Chain->bClosedLoop = true;
				break;
			}
		}

		Chain->Last = LastIndex;
	}

	static void DedupeChains(TArray<TSharedPtr<PCGExCluster::FNodeChain>>& InChains)
	{
		TSet<uint64> Chains;
		Chains.Reserve(InChains.Num() / 2);

		for (int i = 0; i < InChains.Num(); i++)
		{
			const TSharedPtr<PCGExCluster::FNodeChain>& Chain = InChains[i];
			if (!Chain) { continue; }

			bool bAlreadyExists = false;
			Chains.Add(Chain->GetUniqueHash(), &bAlreadyExists);
			if (bAlreadyExists) { InChains[i].Reset(); }
		}
	}
}
