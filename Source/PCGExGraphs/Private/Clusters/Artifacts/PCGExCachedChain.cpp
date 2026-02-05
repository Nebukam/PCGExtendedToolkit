// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExCachedChain.h"

#include "Algo/RemoveIf.h"
#include "Clusters/Artifacts/PCGExChain.h"
#include "Clusters/PCGExCluster.h"
#include "Core/PCGExMTCommon.h"

#define LOCTEXT_NAMESPACE "PCGExCachedChain"

namespace PCGExClusters
{
#pragma region FChainCacheFactory

	FText FChainCacheFactory::GetDisplayName() const
	{
		return LOCTEXT("DisplayName", "Node Chains");
	}

	FText FChainCacheFactory::GetTooltip() const
	{
		return LOCTEXT("Tooltip", "Pre-built node chains for path extraction and cluster simplification operations.");
	}

	TSharedPtr<ICachedClusterData> FChainCacheFactory::Build(const FClusterCacheBuildContext& Context) const
	{
		// Delegate to the shared build function
		return ChainHelpers::BuildAndCacheChains(Context.Cluster);
	}

#pragma endregion

#pragma region ChainHelpers

	namespace ChainHelpers
	{
		bool GetOrBuildChains(
			const TSharedRef<FCluster>& Cluster,
			TArray<TSharedPtr<FNodeChain>>& OutChains,
			const TSharedPtr<TArray<int8>>& Breakpoints,
			const bool bLeavesOnly)
		{
			// Try cache first
			TSharedPtr<FCachedChainData> CachedChains = Cluster->GetCachedData<FCachedChainData>(FChainCacheFactory::CacheKey);

			if (!CachedChains)
			{
				// Cache miss - build and cache
				CachedChains = BuildAndCacheChains(Cluster);
			}

			if (!CachedChains || CachedChains->Chains.IsEmpty())
			{
				OutChains.Empty();
				return false;
			}

			// Apply breakpoints if present
			if (Breakpoints && !Breakpoints->IsEmpty())
			{
				ApplyBreakpoints(CachedChains->Chains, Cluster, Breakpoints, OutChains);
			}
			else
			{
				OutChains = CachedChains->Chains;
			}

			// Filter to leaves if requested
			if (bLeavesOnly)
			{
				FilterLeavesOnly(OutChains, OutChains);
			}

			return !OutChains.IsEmpty();
		}

		TSharedPtr<FCachedChainData> BuildAndCacheChains(const TSharedRef<FCluster>& Cluster)
		{
			// Step 1: Find all chain seeds (starting points)
			TArray<TSharedPtr<FNodeChain>> Chains;
			Chains.Reserve(Cluster->Edges->Num());

			int32 NumBinaries = 0;
			const int32 NumNodes = Cluster->Nodes->Num();

			for (int32 i = 0; i < NumNodes; i++)
			{
				FNode* Node = Cluster->GetNode(i);
				if (!Node || Node->IsEmpty()) { continue; }

				if (Node->IsLeaf())
				{
					Chains.Add(MakeShared<FNodeChain>(FLink(Node->Index, Node->Links[0].Edge)));
					continue;
				}

				if (Node->IsBinary())
				{
					NumBinaries++;
					continue;
				}

				for (const FLink& Lk : Node->Links)
				{
					// Skip immediately known leaves to avoid double-sampling
					if (Cluster->GetNode(Lk.Node)->IsLeaf()) { continue; }
					Chains.Add(MakeShared<FNodeChain>(FLink(Node->Index, Lk.Edge)));
				}
			}

			Chains.Shrink();

			if (Chains.IsEmpty())
			{
				if (NumBinaries > 0 && NumBinaries == NumNodes)
				{
					// Isolated closed loop - all nodes are binary
					Chains.Add(MakeShared<FNodeChain>(Cluster->GetNode(0)->Links[0]));
				}
				else
				{
					return nullptr;
				}
			}

			// Step 2: Build each chain in parallel
			const int32 NumChains = Chains.Num();
			PCGEX_PARALLEL_FOR(NumChains, Chains[i]->BuildChain(Cluster, nullptr);)

			// Step 3: Deduplicate based on unique hash
			TSet<uint64> UniqueHashSet;
			UniqueHashSet.Reserve(NumChains);

			Chains.SetNum(Algo::StableRemoveIf(
				Chains,
				[&UniqueHashSet](const TSharedPtr<FNodeChain>& Chain)
				{
					bool bAlreadySet = false;
					UniqueHashSet.Add(Chain->UniqueHash, &bAlreadySet);
					return bAlreadySet;
				}));

			// Step 4: Create and cache the result
			TSharedPtr<FCachedChainData> Cached = MakeShared<FCachedChainData>();
			Cached->ContextHash = 0; // No context needed - chains depend only on topology
			Cached->Chains = MoveTemp(Chains);

			// Opportunistically cache for downstream consumers
			Cluster->SetCachedData(FChainCacheFactory::CacheKey, Cached);

			return Cached;
		}

		void ApplyBreakpoints(
			const TArray<TSharedPtr<FNodeChain>>& SourceChains,
			const TSharedRef<FCluster>& Cluster,
			const TSharedPtr<TArray<int8>>& Breakpoints,
			TArray<TSharedPtr<FNodeChain>>& OutChains)
		{
			if (!Breakpoints || Breakpoints->IsEmpty())
			{
				// No breakpoints - just copy source chains
				OutChains = SourceChains;
				return;
			}

			OutChains.Reset();
			OutChains.Reserve(SourceChains.Num() * 2); // Estimate some splits

			const TArray<int8>& BreakpointsRef = *Breakpoints;

			for (const TSharedPtr<FNodeChain>& SourceChain : SourceChains)
			{
				if (!SourceChain) { continue; }

					// Single edge chains can't be split - pass through as-is
				// (Breakpoints only meaningfully apply to binary nodes in multi-link chains)
				if (SourceChain->SingleEdge != -1)
				{
					OutChains.Add(SourceChain);
					continue;
				}

				// Walk through the chain and split at breakpoints
				TArray<FLink> CurrentSegmentLinks;
				CurrentSegmentLinks.Reserve(SourceChain->Links.Num());

				int32 SegmentSeedNode = SourceChain->Seed.Node;
				int32 SegmentSeedEdge = SourceChain->Seed.Edge;
				bool bSegmentStartIsBreakpoint = BreakpointsRef.IsValidIndex(Cluster->GetNodePointIndex(SegmentSeedNode)) &&
				                                  BreakpointsRef[Cluster->GetNodePointIndex(SegmentSeedNode)];

				for (int32 i = 0; i < SourceChain->Links.Num(); i++)
				{
					const FLink& Link = SourceChain->Links[i];
					const int32 NodePointIndex = Cluster->GetNodePointIndex(Link.Node);
					const bool bIsBreakpoint = BreakpointsRef.IsValidIndex(NodePointIndex) && BreakpointsRef[NodePointIndex];

					if (bIsBreakpoint)
					{
						// Include this link in the current segment (chain goes TO the breakpoint)
						CurrentSegmentLinks.Add(Link);

						// Emit current segment
						TSharedPtr<FNodeChain> NewChain = MakeShared<FNodeChain>(FLink(SegmentSeedNode, SegmentSeedEdge));
						NewChain->Links = MoveTemp(CurrentSegmentLinks);
						NewChain->bIsClosedLoop = false; // Breakpoints break loops

						// Determine if this segment is a leaf chain (topology only, not breakpoints)
						const FNode* StartNode = Cluster->GetNode(SegmentSeedNode);
						const FNode* EndNode = Cluster->GetNode(NewChain->Links.Last().Node);
						NewChain->bIsLeaf = (StartNode && StartNode->IsLeaf()) || (EndNode && EndNode->IsLeaf());

						NewChain->FixUniqueHash();
						OutChains.Add(NewChain);

						CurrentSegmentLinks.Reset();
						CurrentSegmentLinks.Reserve(SourceChain->Links.Num() - i);

						// Start new segment from breakpoint node
						SegmentSeedNode = Link.Node;
						SegmentSeedEdge = (i + 1 < SourceChain->Links.Num()) ? SourceChain->Links[i + 1].Edge : Link.Edge;
						bSegmentStartIsBreakpoint = true;
					}
					else
					{
						CurrentSegmentLinks.Add(Link);
					}
				}

				// Emit final segment
				if (!CurrentSegmentLinks.IsEmpty())
				{
					TSharedPtr<FNodeChain> NewChain = MakeShared<FNodeChain>(FLink(SegmentSeedNode, SegmentSeedEdge));
					NewChain->Links = MoveTemp(CurrentSegmentLinks);

					// Check for closed loop (only if source was a closed loop and no breakpoints hit)
					NewChain->bIsClosedLoop = SourceChain->bIsClosedLoop &&
					                          SegmentSeedNode == SourceChain->Seed.Node &&
					                          !bSegmentStartIsBreakpoint;

					// Determine if this is a leaf chain (topology only, not breakpoints)
					const FNode* StartNode = Cluster->GetNode(SegmentSeedNode);
					const FNode* EndNode = Cluster->GetNode(NewChain->Links.Last().Node);
					NewChain->bIsLeaf = (StartNode && StartNode->IsLeaf()) || (EndNode && EndNode->IsLeaf());

					if (NewChain->bIsClosedLoop) { NewChain->bIsLeaf = false; }

					NewChain->FixUniqueHash();
					OutChains.Add(NewChain);
				}
			}

			// Deduplicate results
			TSet<uint64> UniqueHashSet;
			UniqueHashSet.Reserve(OutChains.Num());

			OutChains.SetNum(Algo::StableRemoveIf(
				OutChains,
				[&UniqueHashSet](const TSharedPtr<FNodeChain>& Chain)
				{
					if (!Chain || Chain->Links.IsEmpty()) { return true; }
					bool bAlreadySet = false;
					UniqueHashSet.Add(Chain->UniqueHash, &bAlreadySet);
					return bAlreadySet;
				}));
		}

		void FilterLeavesOnly(
			const TArray<TSharedPtr<FNodeChain>>& SourceChains,
			TArray<TSharedPtr<FNodeChain>>& OutChains)
		{
			if (&SourceChains == &OutChains)
			{
				// In-place filter
				OutChains.SetNum(Algo::StableRemoveIf(
					OutChains,
					[](const TSharedPtr<FNodeChain>& Chain) { return !Chain || !Chain->bIsLeaf; }));
			}
			else
			{
				OutChains.Reset();
				OutChains.Reserve(SourceChains.Num());

				for (const TSharedPtr<FNodeChain>& Chain : SourceChains)
				{
					if (Chain && Chain->bIsLeaf)
					{
						OutChains.Add(Chain);
					}
				}
			}
		}
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
