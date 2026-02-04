// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Performance/Stress Tests
 *
 * Tests component behavior under load with large datasets.
 * These tests verify:
 * - Correctness with large data volumes
 * - Memory handling
 * - Algorithm scaling behavior
 *
 * Run these tests:
 * - In Editor: Session Frontend > Automation > Filter "PCGEx.Performance"
 */

#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"

#include "Math/OBB/PCGExOBBCollection.h"
#include "Math/OBB/PCGExOBB.h"
#include "Math/Geo/PCGExDelaunay.h"
#include "Math/Geo/PCGExVoronoi.h"
#include "Clusters/PCGExLink.h"
#include "Clusters/PCGExEdge.h"
#include "Clusters/PCGExNode.h"
#include "Containers/PCGExIndexLookup.h"

//////////////////////////////////////////////////////////////////
// OBB Collection Stress Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfOBBCollectionLargeDataset,
	"PCGEx.Performance.OBBCollection.LargeDataset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfOBBCollectionLargeDataset::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	constexpr int32 NumBoxes = 10000;
	constexpr float Spacing = 100.0f;
	constexpr float BoxSize = 10.0f;

	FCollection Collection;
	Collection.Reserve(NumBoxes);

	const double StartAdd = FPlatformTime::Seconds();

	// Create a 3D grid of boxes
	int32 GridSize = FMath::CeilToInt(FMath::Pow(static_cast<float>(NumBoxes), 1.0f / 3.0f));
	int32 Added = 0;
	for (int32 X = 0; X < GridSize && Added < NumBoxes; X++)
	{
		for (int32 Y = 0; Y < GridSize && Added < NumBoxes; Y++)
		{
			for (int32 Z = 0; Z < GridSize && Added < NumBoxes; Z++)
			{
				const FVector Position(X * Spacing, Y * Spacing, Z * Spacing);
				const FTransform Transform(Position);
				const FBox LocalBox(FVector(-BoxSize), FVector(BoxSize));
				Collection.Add(Transform, LocalBox, Added);
				Added++;
			}
		}
	}

	const double EndAdd = FPlatformTime::Seconds();
	const double AddTime = EndAdd - StartAdd;

	TestEqual(TEXT("All boxes added"), Collection.Num(), NumBoxes);
	AddInfo(FString::Printf(TEXT("Added %d boxes in %.3f ms"), NumBoxes, AddTime * 1000.0));

	// Build octree
	const double StartBuild = FPlatformTime::Seconds();
	Collection.BuildOctree();
	const double EndBuild = FPlatformTime::Seconds();
	const double BuildTime = EndBuild - StartBuild;

	AddInfo(FString::Printf(TEXT("Built octree in %.3f ms"), BuildTime * 1000.0));

	// Test point queries
	constexpr int32 NumQueries = 1000;
	int32 HitCount = 0;

	const double StartQuery = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumQueries; i++)
	{
		// Query points scattered throughout the grid
		const FVector QueryPoint(
			FMath::FRand() * GridSize * Spacing,
			FMath::FRand() * GridSize * Spacing,
			FMath::FRand() * GridSize * Spacing
		);
		if (Collection.IsPointInside(QueryPoint))
		{
			HitCount++;
		}
	}
	const double EndQuery = FPlatformTime::Seconds();
	const double QueryTime = EndQuery - StartQuery;

	AddInfo(FString::Printf(TEXT("Performed %d point queries in %.3f ms (%.1f queries/ms), %d hits"),
		NumQueries, QueryTime * 1000.0, NumQueries / (QueryTime * 1000.0), HitCount));

	// Test OBB overlap queries
	constexpr int32 NumOverlapQueries = 500;
	int32 OverlapHits = 0;

	const double StartOverlap = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumOverlapQueries; i++)
	{
		const FVector QueryPos(
			FMath::FRand() * GridSize * Spacing,
			FMath::FRand() * GridSize * Spacing,
			FMath::FRand() * GridSize * Spacing
		);
		const FOBB Query = Factory::FromAABB(FBox(QueryPos - FVector(BoxSize * 2), QueryPos + FVector(BoxSize * 2)), -1);
		if (Collection.Overlaps(Query))
		{
			OverlapHits++;
		}
	}
	const double EndOverlap = FPlatformTime::Seconds();
	const double OverlapTime = EndOverlap - StartOverlap;

	AddInfo(FString::Printf(TEXT("Performed %d overlap queries in %.3f ms (%.1f queries/ms), %d hits"),
		NumOverlapQueries, OverlapTime * 1000.0, NumOverlapQueries / (OverlapTime * 1000.0), OverlapHits));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfOBBCollectionBulkClassify,
	"PCGEx.Performance.OBBCollection.BulkClassify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfOBBCollectionBulkClassify::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	// Create a moderate collection
	constexpr int32 NumBoxes = 1000;
	FCollection Collection;

	for (int32 i = 0; i < NumBoxes; i++)
	{
		const FVector Position(FMath::FRand() * 1000.0f, FMath::FRand() * 1000.0f, FMath::FRand() * 1000.0f);
		Collection.Add(FTransform(Position), FBox(FVector(-20), FVector(20)), i);
	}
	Collection.BuildOctree();

	// Create large point set to classify
	constexpr int32 NumPoints = 50000;
	TArray<FVector> Points;
	Points.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; i++)
	{
		Points.Add(FVector(FMath::FRand() * 1000.0f, FMath::FRand() * 1000.0f, FMath::FRand() * 1000.0f));
	}

	// Bulk classify
	TBitArray<> InsideMask;

	const double StartClassify = FPlatformTime::Seconds();
	Collection.ClassifyPoints(Points, InsideMask);
	const double EndClassify = FPlatformTime::Seconds();
	const double ClassifyTime = EndClassify - StartClassify;

	TestEqual(TEXT("Mask size matches points"), InsideMask.Num(), NumPoints);

	int32 InsideCount = 0;
	for (int32 i = 0; i < InsideMask.Num(); i++)
	{
		if (InsideMask[i]) InsideCount++;
	}

	AddInfo(FString::Printf(TEXT("Classified %d points against %d boxes in %.3f ms (%.1f points/ms), %d inside"),
		NumPoints, NumBoxes, ClassifyTime * 1000.0, NumPoints / (ClassifyTime * 1000.0), InsideCount));

	return true;
}

//////////////////////////////////////////////////////////////////
// Delaunay/Voronoi 3D Stress Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfDelaunay3DLarge,
	"PCGEx.Performance.Delaunay3D.LargePointSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfDelaunay3DLarge::RunTest(const FString& Parameters)
{
	constexpr int32 NumPoints = 2000;

	// Generate random 3D points
	TArray<FVector> Positions;
	Positions.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; i++)
	{
		Positions.Add(FVector(
			FMath::FRand() * 500.0f,
			FMath::FRand() * 500.0f,
			FMath::FRand() * 500.0f
		));
	}

	PCGExMath::Geo::TDelaunay3 Delaunay;

	const double StartProcess = FPlatformTime::Seconds();
	const bool bSuccess = Delaunay.Process<false, false>(Positions);
	const double EndProcess = FPlatformTime::Seconds();
	const double ProcessTime = EndProcess - StartProcess;

	TestTrue(TEXT("Delaunay 3D succeeded"), bSuccess);
	TestTrue(TEXT("Generated sites"), Delaunay.Sites.Num() > 0);
	TestTrue(TEXT("Generated edges"), Delaunay.DelaunayEdges.Num() > 0);

	AddInfo(FString::Printf(TEXT("Delaunay 3D: %d points -> %d sites, %d edges in %.3f ms"),
		NumPoints, Delaunay.Sites.Num(), Delaunay.DelaunayEdges.Num(), ProcessTime * 1000.0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfVoronoi3DLarge,
	"PCGEx.Performance.Voronoi3D.LargePointSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfVoronoi3DLarge::RunTest(const FString& Parameters)
{
	constexpr int32 NumPoints = 1500;

	// Generate random 3D points
	TArray<FVector> Positions;
	Positions.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; i++)
	{
		Positions.Add(FVector(
			FMath::FRand() * 500.0f,
			FMath::FRand() * 500.0f,
			FMath::FRand() * 500.0f
		));
	}

	PCGExMath::Geo::TVoronoi3 Voronoi;

	const double StartProcess = FPlatformTime::Seconds();
	const bool bSuccess = Voronoi.Process(Positions);
	const double EndProcess = FPlatformTime::Seconds();
	const double ProcessTime = EndProcess - StartProcess;

	TestTrue(TEXT("Voronoi 3D succeeded"), bSuccess);
	TestTrue(TEXT("Generated edges"), Voronoi.VoronoiEdges.Num() > 0);
	TestTrue(TEXT("Generated circumspheres"), Voronoi.Circumspheres.Num() > 0);

	AddInfo(FString::Printf(TEXT("Voronoi 3D: %d points -> %d edges, %d circumspheres in %.3f ms"),
		NumPoints, Voronoi.VoronoiEdges.Num(), Voronoi.Circumspheres.Num(), ProcessTime * 1000.0));

	return true;
}

//////////////////////////////////////////////////////////////////
// Cluster Structure Stress Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfNodeLinkingLarge,
	"PCGEx.Performance.ClusterStructs.LargeGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfNodeLinkingLarge::RunTest(const FString& Parameters)
{
	constexpr int32 NumNodes = 10000;
	constexpr int32 AvgEdgesPerNode = 4;

	TArray<PCGExGraphs::FNode> Nodes;
	Nodes.Reserve(NumNodes);

	// Create nodes
	const double StartCreate = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumNodes; i++)
	{
		Nodes.Emplace(i, i);
	}
	const double EndCreate = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Created %d nodes in %.3f ms"), NumNodes, (EndCreate - StartCreate) * 1000.0));

	// Create random edges
	TArray<PCGExGraphs::FEdge> Edges;
	int32 EdgeIndex = 0;

	const double StartLink = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumNodes; i++)
	{
		// Connect to random neighbors
		int32 NumConnections = FMath::RandRange(1, AvgEdgesPerNode * 2);
		for (int32 j = 0; j < NumConnections; j++)
		{
			int32 TargetNode = FMath::RandRange(0, NumNodes - 1);
			if (TargetNode != i && !Nodes[i].IsAdjacentTo(TargetNode))
			{
				Nodes[i].Link(TargetNode, EdgeIndex);
				Nodes[TargetNode].Link(i, EdgeIndex);
				Edges.Emplace(EdgeIndex, i, TargetNode);
				EdgeIndex++;
			}
		}
	}
	const double EndLink = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Created %d edges in %.3f ms"), Edges.Num(), (EndLink - StartLink) * 1000.0));

	// Test adjacency queries
	constexpr int32 NumQueries = 100000;
	int32 AdjacentCount = 0;

	const double StartQuery = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumQueries; i++)
	{
		int32 NodeA = FMath::RandRange(0, NumNodes - 1);
		int32 NodeB = FMath::RandRange(0, NumNodes - 1);
		if (Nodes[NodeA].IsAdjacentTo(NodeB))
		{
			AdjacentCount++;
		}
	}
	const double EndQuery = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Performed %d adjacency queries in %.3f ms (%.1f queries/ms)"),
		NumQueries, (EndQuery - StartQuery) * 1000.0, NumQueries / ((EndQuery - StartQuery) * 1000.0)));

	// Count node types
	int32 LeafCount = 0, BinaryCount = 0, ComplexCount = 0;
	for (const auto& Node : Nodes)
	{
		if (Node.IsLeaf()) LeafCount++;
		else if (Node.IsBinary()) BinaryCount++;
		else if (Node.IsComplex()) ComplexCount++;
	}

	AddInfo(FString::Printf(TEXT("Node types: %d leaf, %d binary, %d complex"),
		LeafCount, BinaryCount, ComplexCount));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfEdgeHashPerformance,
	"PCGEx.Performance.ClusterStructs.EdgeHashing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfEdgeHashPerformance::RunTest(const FString& Parameters)
{
	constexpr int32 NumEdges = 100000;

	// Create edges and hash them
	TSet<uint64> EdgeSet;
	EdgeSet.Reserve(NumEdges);

	const double StartHash = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumEdges; i++)
	{
		PCGExGraphs::FEdge Edge(i, i, i + 1);
		EdgeSet.Add(Edge.H64U());
	}
	const double EndHash = FPlatformTime::Seconds();

	TestEqual(TEXT("All edges added to set"), EdgeSet.Num(), NumEdges);
	AddInfo(FString::Printf(TEXT("Hashed %d edges in %.3f ms"), NumEdges, (EndHash - StartHash) * 1000.0));

	// Test lookup performance
	constexpr int32 NumLookups = 100000;
	int32 FoundCount = 0;

	const double StartLookup = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumLookups; i++)
	{
		PCGExGraphs::FEdge QueryEdge(0, FMath::RandRange(0, NumEdges * 2), FMath::RandRange(0, NumEdges * 2));
		if (EdgeSet.Contains(QueryEdge.H64U()))
		{
			FoundCount++;
		}
	}
	const double EndLookup = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Performed %d edge lookups in %.3f ms (%.1f lookups/ms), %d found"),
		NumLookups, (EndLookup - StartLookup) * 1000.0, NumLookups / ((EndLookup - StartLookup) * 1000.0), FoundCount));

	return true;
}

//////////////////////////////////////////////////////////////////
// Index Lookup Stress Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfIndexLookupLarge,
	"PCGEx.Performance.IndexLookup.LargeDataset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfIndexLookupLarge::RunTest(const FString& Parameters)
{
	constexpr int32 NumIndices = 100000;

	PCGEx::FIndexLookup Lookup(NumIndices);

	// Initialize with identity mapping
	const double StartInit = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumIndices; i++)
	{
		Lookup[i] = i;
	}
	const double EndInit = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Initialized %d indices in %.3f ms"), NumIndices, (EndInit - StartInit) * 1000.0));

	// Random access pattern
	constexpr int32 NumAccesses = 1000000;
	int64 Sum = 0;

	const double StartAccess = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumAccesses; i++)
	{
		int32 Index = FMath::RandRange(0, NumIndices - 1);
		Sum += Lookup[Index];
	}
	const double EndAccess = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Performed %d random accesses in %.3f ms (%.1f accesses/ms)"),
		NumAccesses, (EndAccess - StartAccess) * 1000.0, NumAccesses / ((EndAccess - StartAccess) * 1000.0)));

	// Prevent optimization from eliminating the loop
	TestTrue(TEXT("Sum is valid"), Sum >= 0);

	return true;
}

//////////////////////////////////////////////////////////////////
// Memory Stress Tests
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfMemoryOBBCollection,
	"PCGEx.Performance.Memory.OBBCollectionGrowth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfMemoryOBBCollection::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	// Test growing collection without pre-reserve
	{
		FCollection Collection;
		const FBox LocalBox(FVector(-10), FVector(10));

		const double StartGrow = FPlatformTime::Seconds();
		for (int32 i = 0; i < 10000; i++)
		{
			Collection.Add(FTransform(FVector(i * 25.0f, 0, 0)), LocalBox, i);
		}
		const double EndGrow = FPlatformTime::Seconds();

		AddInfo(FString::Printf(TEXT("Growing collection (no reserve): 10000 boxes in %.3f ms"), (EndGrow - StartGrow) * 1000.0));
	}

	// Test with pre-reserve
	{
		FCollection Collection;
		Collection.Reserve(10000);
		const FBox LocalBox(FVector(-10), FVector(10));

		const double StartReserved = FPlatformTime::Seconds();
		for (int32 i = 0; i < 10000; i++)
		{
			Collection.Add(FTransform(FVector(i * 25.0f, 0, 0)), LocalBox, i);
		}
		const double EndReserved = FPlatformTime::Seconds();

		AddInfo(FString::Printf(TEXT("Pre-reserved collection: 10000 boxes in %.3f ms"), (EndReserved - StartReserved) * 1000.0));
	}

	// Test reset and reuse
	{
		FCollection Collection;
		Collection.Reserve(5000);
		const FBox LocalBox(FVector(-10), FVector(10));

		const double StartCycles = FPlatformTime::Seconds();
		for (int32 Cycle = 0; Cycle < 10; Cycle++)
		{
			for (int32 i = 0; i < 5000; i++)
			{
				Collection.Add(FTransform(FVector(i * 25.0f, 0, 0)), LocalBox, i);
			}
			Collection.BuildOctree();
			Collection.Reset();
		}
		const double EndCycles = FPlatformTime::Seconds();

		AddInfo(FString::Printf(TEXT("10 cycles of 5000 boxes (add/build/reset): %.3f ms"), (EndCycles - StartCycles) * 1000.0));
	}

	return true;
}

//////////////////////////////////////////////////////////////////
// Concurrent Access Simulation (Single-threaded stress)
//////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPCGExPerfMixedOperations,
	"PCGEx.Performance.MixedOperations.InterleavedQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPCGExPerfMixedOperations::RunTest(const FString& Parameters)
{
	using namespace PCGExMath::OBB;

	// Build a collection
	FCollection Collection;
	Collection.Reserve(1000);

	for (int32 i = 0; i < 1000; i++)
	{
		const FVector Position(
			FMath::FRand() * 500.0f,
			FMath::FRand() * 500.0f,
			FMath::FRand() * 500.0f
		);
		Collection.Add(FTransform(Position), FBox(FVector(-15), FVector(15)), i);
	}
	Collection.BuildOctree();

	// Interleave different query types
	constexpr int32 NumIterations = 10000;
	int32 PointHits = 0, OverlapHits = 0, SegmentHits = 0;

	const double StartTime = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumIterations; i++)
	{
		switch (i % 3)
		{
		case 0:
			{
				const FVector Point(FMath::FRand() * 500.0f, FMath::FRand() * 500.0f, FMath::FRand() * 500.0f);
				if (Collection.IsPointInside(Point)) PointHits++;
			}
			break;
		case 1:
			{
				const FVector Pos(FMath::FRand() * 500.0f, FMath::FRand() * 500.0f, FMath::FRand() * 500.0f);
				const FOBB Query = Factory::FromAABB(FBox(Pos - FVector(20), Pos + FVector(20)), -1);
				if (Collection.Overlaps(Query)) OverlapHits++;
			}
			break;
		case 2:
			{
				const FVector SegStart(FMath::FRand() * 500.0f, FMath::FRand() * 500.0f, FMath::FRand() * 500.0f);
				const FVector SegEnd(FMath::FRand() * 500.0f, FMath::FRand() * 500.0f, FMath::FRand() * 500.0f);
				if (Collection.SegmentIntersectsAny(SegStart, SegEnd)) SegmentHits++;
			}
			break;
		}
	}
	const double EndTime = FPlatformTime::Seconds();

	AddInfo(FString::Printf(TEXT("Mixed operations: %d iterations in %.3f ms"), NumIterations, (EndTime - StartTime) * 1000.0));
	AddInfo(FString::Printf(TEXT("Hits - Point: %d, Overlap: %d, Segment: %d"), PointHits, OverlapHits, SegmentHits));

	return true;
}
