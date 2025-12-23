// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clusters/PCGExLink.h"

namespace PCGExMT
{
	class FTaskManager;
}

namespace PCGExClusters
{
	class FCluster;
	using PCGExGraphs::FLink;

	class PCGEXCORE_API FNodeChain : public TSharedFromThis<FNodeChain>
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

		FVector GetFirstEdgeDir(const TSharedPtr<FCluster>& Cluster) const;
		FVector GetLastEdgeDir(const TSharedPtr<FCluster>& Cluster) const;
		FVector GetEdgeDir(const TSharedPtr<FCluster>& Cluster, const bool bFirst) const;

		int32 GetNodes(const TSharedPtr<FCluster>& Cluster, TArray<int32>& OutNodes, bool bReverse);
	};

	class PCGEXCORE_API FNodeChainBuilder : public TSharedFromThis<FNodeChainBuilder>
	{
	public:
		TSharedRef<FCluster> Cluster;
		TSharedPtr<TArray<int8>> Breakpoints;
		TArray<TSharedPtr<FNodeChain>> Chains;

		FNodeChainBuilder(const TSharedRef<FCluster>& InCluster);

		bool Compile(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager);
		bool CompileLeavesOnly(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager);

	protected:
		bool DispatchTasks(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager);
		void Dedupe();
	};
}
