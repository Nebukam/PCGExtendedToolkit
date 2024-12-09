// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGraph.h"
#include "PCGExCluster.h"

namespace PCGExCluster
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FNodeChain : public TSharedFromThis<FNodeChain>
	{
	public:
		FLink Seed;
		int32 SingleEdge = -1;

		bool bIsClosedLoop = false;
		bool bIsLeaf = false;

		uint64 UniqueHash = 0;
		TArray<FLink> Links; // {Seed} [Edge <- Node][Edge <- Node] // Seed hold edge index that wrap if closed loop.

		explicit FNodeChain(const FLink InSeed)
			: Seed(InSeed)
		{
		}

		~FNodeChain() = default;
		void FixUniqueHash();

		void BuildChain(const TSharedRef<FCluster>& Cluster, const TSharedPtr<TArray<int8>>& Breakpoints);
		void Dump(const TSharedRef<FCluster>& Cluster, const TSharedPtr<PCGExGraph::FGraph>& Graph, const bool bAddMetadata) const;
		void DumpReduced(const TSharedRef<FCluster>& Cluster, const TSharedPtr<PCGExGraph::FGraph>& Graph, const bool bAddMetadata) const;
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FNodeChainBuilder : public TSharedFromThis<FNodeChainBuilder>
	{
	public:
		TSharedRef<FCluster> Cluster;
		TSharedPtr<TArray<int8>> Breakpoints;
		TArray<TSharedPtr<FNodeChain>> Chains;

		FNodeChainBuilder(const TSharedRef<FCluster>& InCluster)
			: Cluster(InCluster)
		{
		}

		bool Compile(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);
		bool CompileLeavesOnly(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);

	protected:
		bool DispatchTasks(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager);
		void Dedupe();
	};
}
