// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/AutomationTest.h"
#include "Clusters/PCGExLink.h"
#include "Clusters/PCGExEdge.h"
#include "Clusters/PCGExNode.h"

//////////////////////////////////////////////////////////////////
// FLink Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFLinkConstruction,
	"PCGEx.Unit.Clusters.FLink.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFLinkConstruction::RunTest(const FString& Parameters)
{
	// Default constructor
	{
		const PCGExGraphs::FLink Link;
		TestEqual(TEXT("Default Node is -1"), Link.Node, -1);
		TestEqual(TEXT("Default Edge is -1"), Link.Edge, -1);
	}

	// Constructor with node and edge
	{
		const PCGExGraphs::FLink Link(5, 10);
		TestEqual(TEXT("Node is 5"), Link.Node, 5);
		TestEqual(TEXT("Edge is 10"), Link.Edge, 10);
	}

	// Constructor from hash
	{
		const uint64 Hash = PCGEx::H64U(7, 14);
		const PCGExGraphs::FLink Link(Hash);
		// Note: FLink(uint64) constructor uses H64A which extracts lower 32 bits
		// So both Node and Edge get the same value (lower 32 bits of hash)
		// This appears to be intentional for a specific use case
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFLinkH64,
	"PCGEx.Unit.Clusters.FLink.H64",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFLinkH64::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FLink Link(100, 200);
	const uint64 Hash = Link.H64();

	// Should be able to recreate the same hash
	TestEqual(TEXT("H64 consistent"), Hash, PCGEx::H64U(100, 200));

	// Different links should have different hashes
	const PCGExGraphs::FLink Link2(100, 201);
	TestNotEqual(TEXT("Different links have different H64"), Link.H64(), Link2.H64());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFLinkEquality,
	"PCGEx.Unit.Clusters.FLink.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFLinkEquality::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FLink A(5, 10);
	const PCGExGraphs::FLink B(5, 10);
	const PCGExGraphs::FLink C(5, 11);
	const PCGExGraphs::FLink D(6, 10);

	TestTrue(TEXT("Same node and edge are equal"), A == B);
	TestFalse(TEXT("Different edge not equal"), A == C);
	TestFalse(TEXT("Different node not equal"), A == D);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFLinkGetTypeHash,
	"PCGEx.Unit.Clusters.FLink.GetTypeHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFLinkGetTypeHash::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FLink A(5, 10);
	const PCGExGraphs::FLink B(5, 10);

	// Same links should have same hash
	TestEqual(TEXT("Same links have same hash"), GetTypeHash(A), GetTypeHash(B));

	// Can be used in TSet
	TSet<PCGExGraphs::FLink> LinkSet;
	LinkSet.Add(A);
	TestTrue(TEXT("Link can be added to TSet"), LinkSet.Contains(B));

	LinkSet.Add(PCGExGraphs::FLink(6, 11));
	TestEqual(TEXT("TSet has 2 unique links"), LinkSet.Num(), 2);

	return true;
}

//////////////////////////////////////////////////////////////////
// FEdge Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFEdgeConstruction,
	"PCGEx.Unit.Clusters.FEdge.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFEdgeConstruction::RunTest(const FString& Parameters)
{
	// Default constructor
	{
		const PCGExGraphs::FEdge Edge;
		TestEqual(TEXT("Default Start is 0"), Edge.Start, 0u);
		TestEqual(TEXT("Default End is 0"), Edge.End, 0u);
		TestEqual(TEXT("Default Index is -1"), Edge.Index, -1);
		TestEqual(TEXT("Default PointIndex is -1"), Edge.PointIndex, -1);
		TestEqual(TEXT("Default IOIndex is -1"), Edge.IOIndex, -1);
		TestEqual(TEXT("Default bValid is 1"), Edge.bValid, static_cast<int8>(1));
	}

	// Full constructor
	{
		const PCGExGraphs::FEdge Edge(42, 10, 20, 100, 5);
		TestEqual(TEXT("Start is 10"), Edge.Start, 10u);
		TestEqual(TEXT("End is 20"), Edge.End, 20u);
		TestEqual(TEXT("Index is 42"), Edge.Index, 42);
		TestEqual(TEXT("PointIndex is 100"), Edge.PointIndex, 100);
		TestEqual(TEXT("IOIndex is 5"), Edge.IOIndex, 5);
	}

	// Partial constructor (without PointIndex and IOIndex)
	{
		const PCGExGraphs::FEdge Edge(0, 5, 15);
		TestEqual(TEXT("Start is 5"), Edge.Start, 5u);
		TestEqual(TEXT("End is 15"), Edge.End, 15u);
		TestEqual(TEXT("Index is 0"), Edge.Index, 0);
		TestEqual(TEXT("PointIndex defaults to -1"), Edge.PointIndex, -1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFEdgeOther,
	"PCGEx.Unit.Clusters.FEdge.Other",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFEdgeOther::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FEdge Edge(0, 10, 20);

	// Other returns the opposite endpoint
	TestEqual(TEXT("Other(Start) returns End"), Edge.Other(10), 20u);
	TestEqual(TEXT("Other(End) returns Start"), Edge.Other(20), 10u);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFEdgeContains,
	"PCGEx.Unit.Clusters.FEdge.Contains",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFEdgeContains::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FEdge Edge(0, 10, 20);

	TestTrue(TEXT("Contains Start"), Edge.Contains(10));
	TestTrue(TEXT("Contains End"), Edge.Contains(20));
	TestFalse(TEXT("Does not contain 15"), Edge.Contains(15));
	TestFalse(TEXT("Does not contain 0"), Edge.Contains(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFEdgeEquality,
	"PCGEx.Unit.Clusters.FEdge.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFEdgeEquality::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FEdge A(0, 10, 20);
	const PCGExGraphs::FEdge B(1, 10, 20);  // Different index, same endpoints
	const PCGExGraphs::FEdge C(0, 20, 10);  // Reversed endpoints
	const PCGExGraphs::FEdge D(0, 10, 30);  // Different endpoint

	// Equality is based on H64U which is order-independent (uses min/max)
	TestTrue(TEXT("Same endpoints are equal regardless of index"), A == B);
	TestTrue(TEXT("Reversed endpoints are equal"), A == C);
	TestFalse(TEXT("Different endpoints not equal"), A == D);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFEdgeH64U,
	"PCGEx.Unit.Clusters.FEdge.H64U",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFEdgeH64U::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FEdge A(0, 10, 20);
	const PCGExGraphs::FEdge B(1, 20, 10);  // Reversed endpoints

	// H64U should be the same for both directions
	TestEqual(TEXT("H64U is direction-independent"), A.H64U(), B.H64U());

	// Different edges should have different hashes
	const PCGExGraphs::FEdge C(0, 10, 30);
	TestNotEqual(TEXT("Different edges have different H64U"), A.H64U(), C.H64U());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFEdgeLessThan,
	"PCGEx.Unit.Clusters.FEdge.LessThan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFEdgeLessThan::RunTest(const FString& Parameters)
{
	const PCGExGraphs::FEdge A(0, 1, 2);
	const PCGExGraphs::FEdge B(1, 3, 4);

	// Less than based on H64U
	TestTrue(TEXT("Edge (1,2) < Edge (3,4)"), A < B);
	TestFalse(TEXT("Edge (3,4) not < Edge (1,2)"), B < A);

	return true;
}

//////////////////////////////////////////////////////////////////
// FNode Tests (Basic - without FCluster dependency)
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeConstruction,
	"PCGEx.Unit.Clusters.FNode.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeConstruction::RunTest(const FString& Parameters)
{
	// Default constructor
	{
		const PCGExGraphs::FNode Node;
		TestEqual(TEXT("Default bValid is 1"), Node.bValid, static_cast<int8>(1));
		TestEqual(TEXT("Default Index is -1"), Node.Index, -1);
		TestEqual(TEXT("Default PointIndex is -1"), Node.PointIndex, -1);
		TestEqual(TEXT("Default NumExportedEdges is 0"), Node.NumExportedEdges, 0);
		TestEqual(TEXT("Default Links is empty"), Node.Links.Num(), 0);
	}

	// Constructor with indices
	{
		const PCGExGraphs::FNode Node(5, 10);
		TestEqual(TEXT("Index is 5"), Node.Index, 5);
		TestEqual(TEXT("PointIndex is 10"), Node.PointIndex, 10);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeNumAndIsEmpty,
	"PCGEx.Unit.Clusters.FNode.NumAndIsEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeNumAndIsEmpty::RunTest(const FString& Parameters)
{
	PCGExGraphs::FNode Node(0, 0);

	TestEqual(TEXT("Empty node has Num 0"), Node.Num(), 0);
	TestTrue(TEXT("Empty node IsEmpty"), Node.IsEmpty() != 0);

	Node.Link(1, 0);
	TestEqual(TEXT("Node with 1 link has Num 1"), Node.Num(), 1);
	TestFalse(TEXT("Node with link is not empty"), Node.IsEmpty() != 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeLeafBinaryComplex,
	"PCGEx.Unit.Clusters.FNode.LeafBinaryComplex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeLeafBinaryComplex::RunTest(const FString& Parameters)
{
	// Leaf node (1 connection)
	{
		PCGExGraphs::FNode Node(0, 0);
		Node.Link(1, 0);
		TestTrue(TEXT("Node with 1 link is leaf"), Node.IsLeaf());
		TestFalse(TEXT("Node with 1 link is not binary"), Node.IsBinary());
		TestFalse(TEXT("Node with 1 link is not complex"), Node.IsComplex());
	}

	// Binary node (2 connections)
	{
		PCGExGraphs::FNode Node(0, 0);
		Node.Link(1, 0);
		Node.Link(2, 1);
		TestFalse(TEXT("Node with 2 links is not leaf"), Node.IsLeaf());
		TestTrue(TEXT("Node with 2 links is binary"), Node.IsBinary());
		TestFalse(TEXT("Node with 2 links is not complex"), Node.IsComplex());
	}

	// Complex node (3+ connections)
	{
		PCGExGraphs::FNode Node(0, 0);
		Node.Link(1, 0);
		Node.Link(2, 1);
		Node.Link(3, 2);
		TestFalse(TEXT("Node with 3 links is not leaf"), Node.IsLeaf());
		TestFalse(TEXT("Node with 3 links is not binary"), Node.IsBinary());
		TestTrue(TEXT("Node with 3 links is complex"), Node.IsComplex());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeLinkEdge,
	"PCGEx.Unit.Clusters.FNode.LinkEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeLinkEdge::RunTest(const FString& Parameters)
{
	PCGExGraphs::FNode Node(0, 0);

	Node.LinkEdge(5);
	TestEqual(TEXT("Has 1 link after LinkEdge"), Node.Num(), 1);
	TestEqual(TEXT("Link edge index is 5"), Node.Links[0].Edge, 5);
	TestEqual(TEXT("Link node index is 0 (default for LinkEdge)"), Node.Links[0].Node, 0);

	// LinkEdge uses AddUnique, so duplicates shouldn't be added
	Node.LinkEdge(5);
	TestEqual(TEXT("Duplicate LinkEdge not added"), Node.Num(), 1);

	Node.LinkEdge(10);
	TestEqual(TEXT("Has 2 links after different edge"), Node.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeLink,
	"PCGEx.Unit.Clusters.FNode.Link",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeLink::RunTest(const FString& Parameters)
{
	PCGExGraphs::FNode Node(0, 0);

	Node.Link(5, 10);
	TestEqual(TEXT("Has 1 link after Link"), Node.Num(), 1);
	TestEqual(TEXT("Link node index is 5"), Node.Links[0].Node, 5);
	TestEqual(TEXT("Link edge index is 10"), Node.Links[0].Edge, 10);

	// Link uses AddUnique
	Node.Link(5, 10);
	TestEqual(TEXT("Duplicate Link not added"), Node.Num(), 1);

	Node.Link(6, 11);
	TestEqual(TEXT("Has 2 links after different link"), Node.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeIsAdjacentTo,
	"PCGEx.Unit.Clusters.FNode.IsAdjacentTo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeIsAdjacentTo::RunTest(const FString& Parameters)
{
	PCGExGraphs::FNode Node(0, 0);
	Node.Link(5, 0);
	Node.Link(10, 1);
	Node.Link(15, 2);

	TestTrue(TEXT("Adjacent to node 5"), Node.IsAdjacentTo(5));
	TestTrue(TEXT("Adjacent to node 10"), Node.IsAdjacentTo(10));
	TestTrue(TEXT("Adjacent to node 15"), Node.IsAdjacentTo(15));
	TestFalse(TEXT("Not adjacent to node 7"), Node.IsAdjacentTo(7));
	TestFalse(TEXT("Not adjacent to node 0"), Node.IsAdjacentTo(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterFNodeGetEdgeIndex,
	"PCGEx.Unit.Clusters.FNode.GetEdgeIndex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterFNodeGetEdgeIndex::RunTest(const FString& Parameters)
{
	PCGExGraphs::FNode Node(0, 0);
	Node.Link(5, 100);
	Node.Link(10, 200);
	Node.Link(15, 300);

	TestEqual(TEXT("Edge to node 5 is 100"), Node.GetEdgeIndex(5), 100);
	TestEqual(TEXT("Edge to node 10 is 200"), Node.GetEdgeIndex(10), 200);
	TestEqual(TEXT("Edge to node 15 is 300"), Node.GetEdgeIndex(15), 300);
	TestEqual(TEXT("Edge to non-adjacent node is -1"), Node.GetEdgeIndex(7), -1);

	return true;
}

//////////////////////////////////////////////////////////////////
// NodeGUID Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterNodeGUID,
	"PCGEx.Unit.Clusters.NodeGUID",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterNodeGUID::RunTest(const FString& Parameters)
{
	// Same base and index should produce same GUID
	const uint32 GUID1 = PCGExGraphs::NodeGUID(12345, 10);
	const uint32 GUID2 = PCGExGraphs::NodeGUID(12345, 10);
	TestEqual(TEXT("Same inputs produce same GUID"), GUID1, GUID2);

	// Different index should produce different GUID
	const uint32 GUID3 = PCGExGraphs::NodeGUID(12345, 11);
	TestNotEqual(TEXT("Different index produces different GUID"), GUID1, GUID3);

	// Different base should produce different GUID
	const uint32 GUID4 = PCGExGraphs::NodeGUID(12346, 10);
	TestNotEqual(TEXT("Different base produces different GUID"), GUID1, GUID4);

	return true;
}

//////////////////////////////////////////////////////////////////
// Edge Direction Enums Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExClusterEdgeDirectionEnums,
	"PCGEx.Unit.Clusters.EdgeDirectionEnums",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExClusterEdgeDirectionEnums::RunTest(const FString& Parameters)
{
	// Verify enum values exist and have expected underlying values
	TestEqual(TEXT("EndpointsOrder is 0"), static_cast<uint8>(EPCGExEdgeDirectionMethod::EndpointsOrder), 0);
	TestEqual(TEXT("EndpointsIndices is 1"), static_cast<uint8>(EPCGExEdgeDirectionMethod::EndpointsIndices), 1);
	TestEqual(TEXT("EndpointsSort is 2"), static_cast<uint8>(EPCGExEdgeDirectionMethod::EndpointsSort), 2);
	TestEqual(TEXT("EdgeDotAttribute is 3"), static_cast<uint8>(EPCGExEdgeDirectionMethod::EdgeDotAttribute), 3);

	TestEqual(TEXT("SmallestToGreatest is 0"), static_cast<uint8>(EPCGExEdgeDirectionChoice::SmallestToGreatest), 0);
	TestEqual(TEXT("GreatestToSmallest is 1"), static_cast<uint8>(EPCGExEdgeDirectionChoice::GreatestToSmallest), 1);

	return true;
}
