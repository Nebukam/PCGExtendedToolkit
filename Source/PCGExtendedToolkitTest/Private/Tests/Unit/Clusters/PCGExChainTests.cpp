// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Helpers/PCGExClusterHelpers.h"
#include "Helpers/PCGExChainTestHelpers.h"

/**
 * Chain Building Tests
 *
 * These tests verify the chain extraction and processing logic for clusters.
 * Chains are continuous paths through binary nodes (nodes with exactly 2 neighbors).
 */

//
// Cluster Builder Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterBuilderLinearChainTest,
	"PCGEx.Unit.Clusters.Builder.LinearChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterBuilderLinearChainTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build a 5-node linear chain: 0-1-2-3-4
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	// Verify structure
	TestTrue(TEXT("Has 5 nodes"), ClusterVerify::HasNodeCount(Cluster, 5));
	TestTrue(TEXT("Has 4 edges"), ClusterVerify::HasEdgeCount(Cluster, 4));

	// Node 0 and 4 should be leaves (1 neighbor each)
	TestTrue(TEXT("Node 0 is leaf"), ClusterVerify::NodeIsLeaf(Cluster, 0));
	TestTrue(TEXT("Node 4 is leaf"), ClusterVerify::NodeIsLeaf(Cluster, 4));

	// Nodes 1, 2, 3 should be binary (2 neighbors each)
	TestTrue(TEXT("Node 1 is binary"), ClusterVerify::NodeIsBinary(Cluster, 1));
	TestTrue(TEXT("Node 2 is binary"), ClusterVerify::NodeIsBinary(Cluster, 2));
	TestTrue(TEXT("Node 3 is binary"), ClusterVerify::NodeIsBinary(Cluster, 3));

	TestEqual(TEXT("2 leaf nodes"), ClusterVerify::CountLeafNodes(Cluster), 2);
	TestEqual(TEXT("3 binary nodes"), ClusterVerify::CountBinaryNodes(Cluster), 3);
	TestEqual(TEXT("0 complex nodes"), ClusterVerify::CountComplexNodes(Cluster), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterBuilderClosedLoopTest,
	"PCGEx.Unit.Clusters.Builder.ClosedLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterBuilderClosedLoopTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build a 6-node closed loop
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithClosedLoop(6)
		.Build();

	// Verify structure
	TestTrue(TEXT("Has 6 nodes"), ClusterVerify::HasNodeCount(Cluster, 6));
	TestTrue(TEXT("Has 6 edges"), ClusterVerify::HasEdgeCount(Cluster, 6));

	// All nodes should be binary in a closed loop
	TestEqual(TEXT("0 leaf nodes"), ClusterVerify::CountLeafNodes(Cluster), 0);
	TestEqual(TEXT("6 binary nodes"), ClusterVerify::CountBinaryNodes(Cluster), 6);
	TestEqual(TEXT("0 complex nodes"), ClusterVerify::CountComplexNodes(Cluster), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterBuilderStarTest,
	"PCGEx.Unit.Clusters.Builder.Star",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterBuilderStarTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build a star with 5 leaves
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithStar(5)
		.Build();

	// Verify structure: 1 center + 5 leaves = 6 nodes, 5 edges
	TestTrue(TEXT("Has 6 nodes"), ClusterVerify::HasNodeCount(Cluster, 6));
	TestTrue(TEXT("Has 5 edges"), ClusterVerify::HasEdgeCount(Cluster, 5));

	// Center node (0) should be complex (5 neighbors)
	TestTrue(TEXT("Node 0 is complex"), ClusterVerify::NodeIsComplex(Cluster, 0));
	TestTrue(TEXT("Node 0 has 5 neighbors"), ClusterVerify::NodeHasNeighborCount(Cluster, 0, 5));

	// All leaf nodes should have 1 neighbor
	for (int32 i = 1; i <= 5; i++)
	{
		TestTrue(FString::Printf(TEXT("Node %d is leaf"), i), ClusterVerify::NodeIsLeaf(Cluster, i));
	}

	TestEqual(TEXT("5 leaf nodes"), ClusterVerify::CountLeafNodes(Cluster), 5);
	TestEqual(TEXT("0 binary nodes"), ClusterVerify::CountBinaryNodes(Cluster), 0);
	TestEqual(TEXT("1 complex node"), ClusterVerify::CountComplexNodes(Cluster), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterBuilderGridTest,
	"PCGEx.Unit.Clusters.Builder.Grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterBuilderGridTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build a 3x3 grid
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithGrid(3, 3)
		.Build();

	// 9 nodes, 12 edges (6 horizontal + 6 vertical)
	TestTrue(TEXT("Has 9 nodes"), ClusterVerify::HasNodeCount(Cluster, 9));
	TestTrue(TEXT("Has 12 edges"), ClusterVerify::HasEdgeCount(Cluster, 12));

	// Corners (4) have 2 neighbors, edges (4) have 3 neighbors, center (1) has 4 neighbors
	TestEqual(TEXT("0 leaf nodes"), ClusterVerify::CountLeafNodes(Cluster), 0);
	TestEqual(TEXT("4 binary nodes (corners)"), ClusterVerify::CountBinaryNodes(Cluster), 4);
	TestEqual(TEXT("5 complex nodes"), ClusterVerify::CountComplexNodes(Cluster), 5);

	return true;
}

//
// Chain Building Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBuildLinearTest,
	"PCGEx.Unit.Clusters.Chain.BuildLinear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBuildLinearTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Chains built successfully"), bBuilt);
	TestEqual(TEXT("1 unique chain"), Chains.Num(), 1);

	if (Chains.Num() > 0)
	{
		const TSharedPtr<FTestChain>& Chain = Chains[0];
		TestTrue(TEXT("Chain is leaf"), Chain->bIsLeaf);
		TestFalse(TEXT("Chain is not closed loop"), Chain->bIsClosedLoop);
		TestEqual(TEXT("Chain has 4 links"), Chain->Links.Num(), 4);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBuildClosedLoopTest,
	"PCGEx.Unit.Clusters.Chain.BuildClosedLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBuildClosedLoopTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Closed loop with 6 nodes
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithClosedLoop(6)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Chains built successfully"), bBuilt);
	TestEqual(TEXT("1 unique chain"), Chains.Num(), 1);

	if (Chains.Num() > 0)
	{
		const TSharedPtr<FTestChain>& Chain = Chains[0];
		TestFalse(TEXT("Chain is not leaf"), Chain->bIsLeaf);
		TestTrue(TEXT("Chain is closed loop"), Chain->bIsClosedLoop);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBuildStarTest,
	"PCGEx.Unit.Clusters.Chain.BuildStar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBuildStarTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Star topology: center (0) with 5 leaves
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithStar(5)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Chains built successfully"), bBuilt);
	TestEqual(TEXT("5 chains (one per leaf)"), Chains.Num(), 5);

	// All chains should be single-edge leaf chains
	TestEqual(TEXT("5 single-edge chains"), TestChainHelpers::CountSingleEdgeChains(Chains), 5);
	TestEqual(TEXT("5 leaf chains"), TestChainHelpers::CountLeafChains(Chains), 5);
	TestEqual(TEXT("0 closed loops"), TestChainHelpers::CountClosedLoops(Chains), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBuildBranchingTest,
	"PCGEx.Unit.Clusters.Chain.BuildBranching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBuildBranchingTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Branching topology:
	//     1
	//    /
	// 0-2-3-4
	//    \
	//     5
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.AddNode(0, FVector(0, 0, 0))
		.AddNode(1, FVector(100, 100, 0))
		.AddNode(2, FVector(100, 0, 0))
		.AddNode(3, FVector(200, 0, 0))
		.AddNode(4, FVector(300, 0, 0))
		.AddNode(5, FVector(100, -100, 0))
		.AddEdge(0, 2)
		.AddEdge(2, 1)
		.AddEdge(2, 3)
		.AddEdge(2, 5)
		.AddEdge(3, 4)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Expected chains:
	// - 0-2 (leaf from 0)
	// - 2-1 (leaf from 1, single edge)
	// - 2-3-4 (leaf to 4)
	// - 2-5 (leaf from 5, single edge)
	// Deduplicated, we should have 4 unique chains
	TestEqual(TEXT("4 unique chains"), Chains.Num(), 4);

	// All chains should be leaf chains (all terminate at leaves or the complex node)
	TestEqual(TEXT("4 leaf chains"), TestChainHelpers::CountLeafChains(Chains), 4);

	return true;
}

//
// Breakpoint Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointMiddleTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.Middle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointMiddleTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4 with breakpoint at node 2
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	// Create breakpoints array
	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 5);
	(*Breakpoints)[2] = 1; // Breakpoint at node 2

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Should split into: 0-1-2 and 2-3-4
	TestEqual(TEXT("2 chains after breakpoint"), Chains.Num(), 2);
	TestEqual(TEXT("2 leaf chains"), TestChainHelpers::CountLeafChains(Chains), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointFirstTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.First",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointFirstTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4 with breakpoint at node 1 (first binary node)
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 5);
	(*Breakpoints)[1] = 1; // Breakpoint at node 1

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Should split into: 0-1 and 1-2-3-4
	TestEqual(TEXT("2 chains after breakpoint"), Chains.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointMultipleTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.Multiple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointMultipleTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4-5-6 with breakpoints at nodes 2 and 4
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(7)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 7);
	(*Breakpoints)[2] = 1; // Breakpoint at node 2
	(*Breakpoints)[4] = 1; // Breakpoint at node 4

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Should split into: 0-1-2, 2-3-4, 4-5-6
	TestEqual(TEXT("3 chains after breakpoints"), Chains.Num(), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointConsecutiveTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.Consecutive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointConsecutiveTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4 with consecutive breakpoints at nodes 1 and 2
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 5);
	(*Breakpoints)[1] = 1;
	(*Breakpoints)[2] = 1;

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Should split into: 0-1, 1-2, 2-3-4
	TestEqual(TEXT("3 chains after consecutive breakpoints"), Chains.Num(), 3);

	return true;
}

//
// Leaf Filtering Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainFilterLeavesOnlyTest,
	"PCGEx.Unit.Clusters.Chain.FilterLeavesOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainFilterLeavesOnlyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Branching topology with some leaf chains and some non-leaf chains
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.AddNode(0, FVector(0, 0, 0))
		.AddNode(1, FVector(100, 100, 0))
		.AddNode(2, FVector(100, 0, 0))
		.AddNode(3, FVector(200, 0, 0))
		.AddNode(4, FVector(300, 0, 0))
		.AddNode(5, FVector(100, -100, 0))
		.AddEdge(0, 2)
		.AddEdge(2, 1)
		.AddEdge(2, 3)
		.AddEdge(2, 5)
		.AddEdge(3, 4)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	const int32 TotalChains = Chains.Num();
	const int32 LeafChainsBefore = TestChainHelpers::CountLeafChains(Chains);

	// Filter to leaves only
	TestChainHelpers::FilterLeavesOnly(Chains, Chains);

	TestEqual(TEXT("All remaining chains are leaf chains"), Chains.Num(), LeafChainsBefore);

	// Verify all remaining chains are actually leaves
	for (const TSharedPtr<FTestChain>& Chain : Chains)
	{
		TestTrue(TEXT("Chain is leaf"), Chain->bIsLeaf);
	}

	return true;
}

//
// Deduplication Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainDeduplicationTest,
	"PCGEx.Unit.Clusters.Chain.Deduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainDeduplicationTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: Seeds from both ends should produce same unique chain after dedup
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	// Should deduplicate to 1 chain even though we seed from both leaf nodes
	TestEqual(TEXT("1 unique chain after deduplication"), Chains.Num(), 1);

	return true;
}

//
// Edge Cases
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainSingleEdgeTest,
	"PCGEx.Unit.Clusters.Chain.SingleEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainSingleEdgeTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Single edge: 0-1
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.AddNode(0, FVector(0, 0, 0))
		.AddNode(1, FVector(100, 0, 0))
		.AddEdge(0, 1)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Chain built successfully"), bBuilt);
	TestEqual(TEXT("1 chain"), Chains.Num(), 1);

	if (Chains.Num() > 0)
	{
		TestNotEqual(TEXT("Is single-edge chain"), Chains[0]->SingleEdge, -1);
		TestTrue(TEXT("Is leaf chain"), Chains[0]->bIsLeaf);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainTwoNodesClosedLoopTest,
	"PCGEx.Unit.Clusters.Chain.TwoNodeClosedLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainTwoNodesClosedLoopTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Two nodes with two edges between them (parallel edges)
	// This creates a "loop" with binary nodes
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithClosedLoop(2)
		.Build();

	// With 2 nodes and 2 edges, both nodes are binary
	TestEqual(TEXT("2 nodes"), Cluster->Nodes->Num(), 2);
	TestEqual(TEXT("2 edges"), Cluster->Edges->Num(), 2);
	TestEqual(TEXT("2 binary nodes"), ClusterVerify::CountBinaryNodes(Cluster), 2);

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Chains built successfully"), bBuilt);
	TestEqual(TEXT("1 closed loop chain"), Chains.Num(), 1);

	if (Chains.Num() > 0)
	{
		TestTrue(TEXT("Is closed loop"), Chains[0]->bIsClosedLoop);
	}

	return true;
}
