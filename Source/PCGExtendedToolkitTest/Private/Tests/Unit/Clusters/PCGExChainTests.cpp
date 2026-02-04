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

//
// Breakpoint on Closed Loop Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointClosedLoopTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.ClosedLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointClosedLoopTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Closed loop with 6 nodes, breakpoint at node 3
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithClosedLoop(6)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 6);
	(*Breakpoints)[3] = 1; // Breakpoint at node 3

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Breaking a closed loop at one point should produce one open chain
	// (or two chains depending on how you count the split)
	TestTrue(TEXT("At least 1 chain after breaking loop"), Chains.Num() >= 1);

	// After breaking, no chains should be closed loops
	TestEqual(TEXT("0 closed loops after breakpoint"), TestChainHelpers::CountClosedLoops(Chains), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointClosedLoopMultipleTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.ClosedLoopMultiple",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointClosedLoopMultipleTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Closed loop with 8 nodes, breakpoints at nodes 2 and 6
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithClosedLoop(8)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 8);
	(*Breakpoints)[2] = 1;
	(*Breakpoints)[6] = 1;

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	TestTrue(TEXT("Chains built successfully"), bBuilt);

	// Two breakpoints on a closed loop should create 2 separate chains
	TestEqual(TEXT("2 chains after 2 breakpoints on loop"), Chains.Num(), 2);
	TestEqual(TEXT("0 closed loops"), TestChainHelpers::CountClosedLoops(Chains), 0);

	return true;
}

//
// Breakpoint Ignored Cases
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointAtLeafIgnoredTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.AtLeafIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointAtLeafIgnoredTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4, breakpoint at leaf node 0
	// Breakpoints at leaves should not affect chain building (leaves aren't walked through)
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 5);
	(*Breakpoints)[0] = 1; // Breakpoint at leaf node 0

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	// Breakpoint at leaf shouldn't split - chain still goes 0-1-2-3-4
	// The chain will be split when it REACHES a breakpoint, not starts from one
	TestEqual(TEXT("1 chain (leaf breakpoint doesn't split)"), Chains.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainBreakpointAtComplexIgnoredTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.AtComplexIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainBreakpointAtComplexIgnoredTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Star with center node 0 (complex) and 4 leaves
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithStar(4)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 5);
	(*Breakpoints)[0] = 1; // Breakpoint at complex center node

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	// Complex nodes naturally terminate chains, so breakpoint has no effect
	TestEqual(TEXT("4 chains (same as without breakpoint)"), Chains.Num(), 4);

	return true;
}

//
// Chain Node Order Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainNodeOrderTest,
	"PCGEx.Unit.Clusters.Chain.NodeOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainNodeOrderTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	TestEqual(TEXT("1 chain"), Chains.Num(), 1);

	if (Chains.Num() > 0)
	{
		TArray<int32> NodeIndices;
		Chains[0]->GetNodeIndices(NodeIndices);

		TestEqual(TEXT("Chain has 5 nodes"), NodeIndices.Num(), 5);

		// Verify nodes are sequential (either 0-1-2-3-4 or 4-3-2-1-0)
		bool bForward = (NodeIndices[0] == 0);
		for (int32 i = 0; i < NodeIndices.Num(); i++)
		{
			const int32 Expected = bForward ? i : (4 - i);
			TestEqual(FString::Printf(TEXT("Node %d in order"), i), NodeIndices[i], Expected);
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainNodeOrderReverseTest,
	"PCGEx.Unit.Clusters.Chain.NodeOrderReverse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainNodeOrderReverseTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	if (Chains.Num() > 0)
	{
		TArray<int32> Forward, Reverse;
		Chains[0]->GetNodeIndices(Forward, false);
		Chains[0]->GetNodeIndices(Reverse, true);

		TestEqual(TEXT("Same node count"), Forward.Num(), Reverse.Num());

		// Reverse should be opposite order
		for (int32 i = 0; i < Forward.Num(); i++)
		{
			TestEqual(
				FString::Printf(TEXT("Reverse[%d] == Forward[%d]"), i, Forward.Num() - 1 - i),
				Reverse[i],
				Forward[Forward.Num() - 1 - i]);
		}
	}

	return true;
}

//
// UniqueHash Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainUniqueHashDeterministicTest,
	"PCGEx.Unit.Clusters.Chain.UniqueHash.Deterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainUniqueHashDeterministicTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build same cluster twice
	TSharedRef<FTestCluster> Cluster1 = FClusterBuilder().WithLinearChain(5).Build();
	TSharedRef<FTestCluster> Cluster2 = FClusterBuilder().WithLinearChain(5).Build();

	TArray<TSharedPtr<FTestChain>> Chains1, Chains2;
	TestChainHelpers::BuildChains(Cluster1, Chains1);
	TestChainHelpers::BuildChains(Cluster2, Chains2);

	TestEqual(TEXT("Same chain count"), Chains1.Num(), Chains2.Num());

	if (Chains1.Num() > 0 && Chains2.Num() > 0)
	{
		TestEqual(TEXT("Same UniqueHash"), Chains1[0]->UniqueHash, Chains2[0]->UniqueHash);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainUniqueHashDifferentTest,
	"PCGEx.Unit.Clusters.Chain.UniqueHash.Different",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainUniqueHashDifferentTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build different topologies
	TSharedRef<FTestCluster> Linear = FClusterBuilder().WithLinearChain(5).Build();
	TSharedRef<FTestCluster> Star = FClusterBuilder().WithStar(4).Build();

	TArray<TSharedPtr<FTestChain>> LinearChains, StarChains;
	TestChainHelpers::BuildChains(Linear, LinearChains);
	TestChainHelpers::BuildChains(Star, StarChains);

	if (LinearChains.Num() > 0 && StarChains.Num() > 0)
	{
		TestNotEqual(TEXT("Different UniqueHash"), LinearChains[0]->UniqueHash, StarChains[0]->UniqueHash);
	}

	return true;
}

//
// Complex Topology Tests
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainLadderTopologyTest,
	"PCGEx.Unit.Clusters.Chain.Topology.Ladder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainLadderTopologyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Ladder topology:
	// 0-1-2-3
	// | | | |
	// 4-5-6-7
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.AddNode(0, FVector(0, 0, 0))
		.AddNode(1, FVector(100, 0, 0))
		.AddNode(2, FVector(200, 0, 0))
		.AddNode(3, FVector(300, 0, 0))
		.AddNode(4, FVector(0, 100, 0))
		.AddNode(5, FVector(100, 100, 0))
		.AddNode(6, FVector(200, 100, 0))
		.AddNode(7, FVector(300, 100, 0))
		// Top row
		.AddEdge(0, 1).AddEdge(1, 2).AddEdge(2, 3)
		// Bottom row
		.AddEdge(4, 5).AddEdge(5, 6).AddEdge(6, 7)
		// Rungs
		.AddEdge(0, 4).AddEdge(1, 5).AddEdge(2, 6).AddEdge(3, 7)
		.Build();

	// Corner nodes (0, 3, 4, 7) have 2 neighbors (binary)
	// Middle nodes (1, 2, 5, 6) have 3 neighbors (complex)
	TestEqual(TEXT("4 binary nodes (corners)"), ClusterVerify::CountBinaryNodes(Cluster), 4);
	TestEqual(TEXT("4 complex nodes (middle)"), ClusterVerify::CountComplexNodes(Cluster), 4);

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	// Each corner should generate a single-edge chain to its complex neighbor
	// Deduplication should reduce this
	TestTrue(TEXT("Chains built"), Chains.Num() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainTreeTopologyTest,
	"PCGEx.Unit.Clusters.Chain.Topology.Tree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainTreeTopologyTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Binary tree:
	//       0
	//      / \
	//     1   2
	//    / \
	//   3   4
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.AddNode(0, FVector(100, 0, 0))
		.AddNode(1, FVector(50, 100, 0))
		.AddNode(2, FVector(150, 100, 0))
		.AddNode(3, FVector(25, 200, 0))
		.AddNode(4, FVector(75, 200, 0))
		.AddEdge(0, 1).AddEdge(0, 2)
		.AddEdge(1, 3).AddEdge(1, 4)
		.Build();

	// Node 0: 2 neighbors (binary)
	// Node 1: 3 neighbors (complex)
	// Node 2, 3, 4: 1 neighbor each (leaves)
	TestEqual(TEXT("3 leaf nodes"), ClusterVerify::CountLeafNodes(Cluster), 3);
	TestEqual(TEXT("1 binary node"), ClusterVerify::CountBinaryNodes(Cluster), 1);
	TestEqual(TEXT("1 complex node"), ClusterVerify::CountComplexNodes(Cluster), 1);

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	// Expected chains: 2->0->1, 3->1, 4->1 (all leaf chains)
	TestEqual(TEXT("3 leaf chains"), TestChainHelpers::CountLeafChains(Chains), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainHubAndSpokeWithChainsTest,
	"PCGEx.Unit.Clusters.Chain.Topology.HubWithChains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainHubAndSpokeWithChainsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Hub with chains extending from it:
	// 1-2-3
	//     |
	// 4-5-0-6-7
	//     |
	// 8-9-10
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.AddNode(0, FVector(0, 0, 0))      // Hub (4 neighbors)
		.AddNode(1, FVector(-200, -100, 0))
		.AddNode(2, FVector(-100, -100, 0))
		.AddNode(3, FVector(0, -100, 0))
		.AddNode(4, FVector(-200, 0, 0))
		.AddNode(5, FVector(-100, 0, 0))
		.AddNode(6, FVector(100, 0, 0))
		.AddNode(7, FVector(200, 0, 0))
		.AddNode(8, FVector(-200, 100, 0))
		.AddNode(9, FVector(-100, 100, 0))
		.AddNode(10, FVector(0, 100, 0))
		// Top chain
		.AddEdge(1, 2).AddEdge(2, 3).AddEdge(3, 0)
		// Left chain
		.AddEdge(4, 5).AddEdge(5, 0)
		// Right chain
		.AddEdge(0, 6).AddEdge(6, 7)
		// Bottom chain
		.AddEdge(0, 10).AddEdge(10, 9).AddEdge(9, 8)
		.Build();

	TestTrue(TEXT("Hub is complex"), ClusterVerify::NodeIsComplex(Cluster, 0));

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains);

	// 4 chains radiating from hub, all leaf chains
	TestEqual(TEXT("4 chains from hub"), Chains.Num(), 4);
	TestEqual(TEXT("4 leaf chains"), TestChainHelpers::CountLeafChains(Chains), 4);

	return true;
}

//
// Long Chain Performance Test
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainLongChainTest,
	"PCGEx.Unit.Clusters.Chain.Performance.LongChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainLongChainTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Build a very long chain (1000 nodes)
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(1000)
		.Build();

	TestEqual(TEXT("1000 nodes"), Cluster->Nodes->Num(), 1000);
	TestEqual(TEXT("999 edges"), Cluster->Edges->Num(), 999);

	TArray<TSharedPtr<FTestChain>> Chains;
	const bool bBuilt = TestChainHelpers::BuildChains(Cluster, Chains);

	TestTrue(TEXT("Long chain built"), bBuilt);
	TestEqual(TEXT("1 chain"), Chains.Num(), 1);

	if (Chains.Num() > 0)
	{
		TestEqual(TEXT("Chain has 999 links"), Chains[0]->Links.Num(), 999);
		TestTrue(TEXT("Is leaf chain"), Chains[0]->bIsLeaf);
	}

	return true;
}

//
// All Breakpoints Test
//

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExChainAllBreakpointsTest,
	"PCGEx.Unit.Clusters.Chain.Breakpoint.AllBinary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExChainAllBreakpointsTest::RunTest(const FString& Parameters)
{
	using namespace PCGExTest;

	// Linear chain: 0-1-2-3-4 with breakpoints at ALL binary nodes (1, 2, 3)
	TSharedRef<FTestCluster> Cluster = FClusterBuilder()
		.WithLinearChain(5)
		.Build();

	TSharedPtr<TArray<int8>> Breakpoints = MakeShared<TArray<int8>>();
	Breakpoints->Init(0, 5);
	(*Breakpoints)[1] = 1;
	(*Breakpoints)[2] = 1;
	(*Breakpoints)[3] = 1;

	TArray<TSharedPtr<FTestChain>> Chains;
	TestChainHelpers::BuildChains(Cluster, Chains, Breakpoints);

	// Should split into: 0-1, 1-2, 2-3, 3-4 (4 single-edge chains)
	TestEqual(TEXT("4 chains when all binary nodes are breakpoints"), Chains.Num(), 4);
	TestEqual(TEXT("4 single-edge chains"), TestChainHelpers::CountSingleEdgeChains(Chains), 4);

	return true;
}
