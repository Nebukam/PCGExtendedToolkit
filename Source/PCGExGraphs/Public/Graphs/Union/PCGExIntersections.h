// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExOctree.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExOpStats.h"

#include "Data/PCGExPointElements.h"
#include "Clusters/PCGExEdge.h"
#include "Data/Utils/PCGExDataForwardDetails.h"
#include "Details/PCGExFuseDetails.h"

struct FPCGExEdgeEdgeIntersectionDetails;
struct FPCGExPointEdgeIntersectionDetails;

namespace PCGExBlending
{
	class FMetadataBlender;
}

namespace PCGExData
{
	class FPointIOCollection;
	class FUnionMetadata;
	class IUnionData;
}

enum class EPCGExCutType : uint8;

namespace PCGExMath
{
	struct FCut;
}

namespace PCGExMT
{
	template <typename T>
	class TScopedArray;
}

namespace PCGExGraphs
{
	class FEdgeProxy;
	class FEdgeEdgeIntersections;
	class FGraph;
	struct FEdge;

#pragma region Compound Graph

	/**
	 * Node in a union graph representing one or more fused source points.
	 * 
	 * THREAD SAFETY (Build Phase):
	 * - Add() uses atomic append to internal buffer - thread-safe
	 * - Multiple threads can call Add() concurrently
	 * 
	 * THREAD SAFETY (After Finalize):
	 * - Adjacency set is immutable and safe for concurrent read
	 * - All other members are read-only
	 * 
	 * LIFECYCLE:
	 * 1. Create node (allocates adjacency buffer)
	 * 2. Call Add() from multiple threads during edge insertion
	 * 3. Call Finalize() once all insertions complete (compacts buffer to TSet)
	 * 4. Read Adjacency freely
	 */
	class PCGEXGRAPHS_API FUnionNode : public TSharedFromThis<FUnionNode>
	{
	public:
		/** Original point this node was created from */
		const PCGExData::FConstPoint Point;

		/** Center position (may be updated after fusing) */
		FVector Center;

		/** Bounds of the node */
		FBoxSphereBounds Bounds;

		/** Index in the parent graph's Nodes array */
		int32 Index;

		/** 
		 * Final adjacency set - contains indices of connected nodes.
		 * Only valid after Finalize() is called.
		 */
		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> Adjacency;

		FUnionNode(const PCGExData::FConstPoint& InPoint, const FVector& InCenter, const int32 InIndex);
		~FUnionNode() = default;

		/**
		 * Update center position based on all fused points.
		 * Should be called after all insertions complete.
		 */
		FVector UpdateCenter(
			const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata,
			const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup);

		/**
		 * Add an adjacent node index. Thread-safe during build phase.
		 * Uses atomic append to internal buffer.
		 * @param InAdjacency Index of adjacent node
		 */
		void Add(int32 InAdjacency);

		/**
		 * Finalize the node by compacting the adjacency buffer into the TSet.
		 * MUST be called after all Add() operations complete.
		 * NOT THREAD SAFE with respect to Add() - ensure all adds are done.
		 * Safe to call from parallel loop (each node finalizes independently).
		 */
		void Finalize();

		/** Returns true if Finalize() has been called */
		FORCEINLINE bool IsFinalized() const { return bFinalized; }

	private:
		/** Build-phase buffer for lock-free adjacency collection */
		TArray<int32> AdjacencyBuffer;

		/** Current write position in buffer (atomic for thread safety) */
		std::atomic<int32> AdjacencyWriteIndex{0};

		/** Whether Finalize() has been called */
		bool bFinalized = false;

		/** Lock for buffer growth (rarely needed) */
		mutable FCriticalSection GrowthLock;
	};

	PCGEX_OCTREE_SEMANTICS(FUnionNode, { return Element->Bounds; }, { return A->Index == B->Index; })

	/**
	 * Graph structure for fusing multiple cluster graphs together.
	 * Handles point deduplication and edge merging with union metadata tracking.
	 * 
	 * USAGE PATTERN:
	 * 1. Create graph with fuse settings and bounds
	 * 2. Call Init() and Reserve()
	 * 3. Call BeginConcurrentBuild() before parallel insertions
	 * 4. Insert points/edges (thread-safe methods or _Unsafe for single-threaded)
	 * 5. Call EndConcurrentBuild() when done
	 * 6. Process the fused graph
	 * 
	 * THREAD SAFETY:
	 * - InsertPoint/InsertEdge: Thread-safe (use graph-level locks + builder)
	 * - InsertPoint_Unsafe/InsertEdge_Unsafe: NOT thread-safe (single-threaded only)
	 */
	class PCGEXGRAPHS_API FUnionGraph : public TSharedFromThis<FUnionGraph>
	{
		int32 NumCollapsedEdges = 0;

	public:
		/** Sharded hash map for concurrent node lookup (grid-based fusing) */
		PCGExMT::TH64MapShards<int32> NodeBinsShards;

		/** Non-sharded node map for single-threaded mode */
		TMap<uint64, int32> NodeBins;

		/** Weak reference to source point collection */
		TWeakPtr<PCGExData::FPointIOCollection> SourceCollection = nullptr;

		/** Union metadata for nodes - tracks which source points fused together */
		TSharedPtr<PCGExData::FUnionMetadata> NodesUnion;

		/** Union metadata for edges - tracks which source edges fused together */
		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;

		/** All nodes in the fused graph */
		TArray<TSharedPtr<FUnionNode>> Nodes;

		/** Sharded edge hash map for concurrent edge lookup */
		PCGExMT::TH64MapShards<int32> EdgesMapShards;

		/** Non-sharded edge map for single-threaded mode */
		TMap<uint64, int32> EdgesMap;

		/** All unique edges in the fused graph */
		TArray<FEdge> Edges;

		/** Fuse settings (tolerance, method, etc.) */
		FPCGExFuseDetails FuseDetails;

		/** Spatial bounds of the graph */
		FBox Bounds;

		/** Optional octree for tolerance-based fusing (when not using grid) */
		TUniquePtr<FUnionNodeOctree> Octree;

		/** Lock for node insertion coordination */
		mutable FRWLock UnionLock;

		/** Lock for edge insertion coordination */
		mutable FRWLock EdgesLock;

		explicit FUnionGraph(
			const FPCGExFuseDetails& InFuseDetails,
			const FBox& InBounds,
			const TSharedPtr<PCGExData::FPointIOCollection>& InSourceCollection = nullptr);

		~FUnionGraph() = default;

		/**
		 * Initialize the graph. Must be called before insertions.
		 */
		bool Init(FPCGExContext* InContext);

		/**
		 * Initialize with a specific source facade for local fuse distance support.
		 */
		bool Init(
			FPCGExContext* InContext,
			const TSharedPtr<PCGExData::FFacade>& InUniqueSourceFacade,
			bool SupportScopedGet);

		/**
		 * Pre-allocate memory for expected node and edge counts.
		 * Call before insertions for better performance.
		 * @param NodeReserve Expected number of unique nodes
		 * @param EdgeReserve Expected number of unique edges (-1 to use NodeReserve)
		 */
		void Reserve(int32 NodeReserve, int32 EdgeReserve);

		/** Returns the number of edges after Collapse() */
		FORCEINLINE int32 GetNumCollapsedEdges() const { return NumCollapsedEdges; }

		//=== Concurrent Build Mode ===//

		/**
		 * Begin concurrent build phase for union metadata.
		 * Call before parallel InsertPoint/InsertEdge operations.
		 * @param NodeReserve Expected node count
		 * @param EdgeReserve Expected edge count
		 */
		void BeginConcurrentBuild(int32 NodeReserve = 0, int32 EdgeReserve = 0);

		/**
		 * End concurrent build phase and finalize all data structures.
		 * Finalizes union metadata and node adjacency sets.
		 * Call after all insertions complete, before reading results.
		 */
		void EndConcurrentBuild();

		//=== Point Insertion ===//

		/**
		 * Insert a point, fusing with existing if within tolerance. Thread-safe.
		 * @param Point Point to insert
		 * @return Index of the (possibly existing) node
		 */
		int32 InsertPoint(const PCGExData::FConstPoint& Point);

		/**
		 * Insert a point without thread safety. Use only in single-threaded context.
		 */
		int32 InsertPoint_Unsafe(const PCGExData::FConstPoint& Point);

		//=== Edge Insertion ===//

		/**
		 * Insert an edge between two points. Thread-safe.
		 * Points are inserted if not already present.
		 * @param From Start point
		 * @param To End point
		 * @param Edge Optional edge point data for metadata
		 */
		void InsertEdge(
			const PCGExData::FConstPoint& From,
			const PCGExData::FConstPoint& To,
			const PCGExData::FConstPoint& Edge = PCGExData::NONE_ConstPoint);

		/**
		 * Insert an edge without thread safety. Use only in single-threaded context.
		 */
		void InsertEdge_Unsafe(
			const PCGExData::FConstPoint& From,
			const PCGExData::FConstPoint& To,
			const PCGExData::FConstPoint& Edge = PCGExData::NONE_ConstPoint);

		//=== Results ===//

		/**
		 * Extract unique edges and clear internal edge storage.
		 */
		void GetUniqueEdges(TArray<FEdge>& OutEdges);

		/**
		 * Write node union metadata to a graph structure.
		 */
		void WriteNodeMetadata(const TSharedPtr<FGraph>& InGraph) const;

		/**
		 * Write edge union metadata to a graph structure.
		 */
		void WriteEdgeMetadata(const TSharedPtr<FGraph>& InGraph) const;

		/**
		 * Collapse edge maps and finalize edge count.
		 * Call after all insertions but before reading edges.
		 */
		void Collapse();
	};

#pragma endregion

	class PCGEXGRAPHS_API FIntersectionCache : public TSharedFromThis<FIntersectionCache>
	{
	public:
		TConstPCGValueRange<FTransform> NodeTransforms;
		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FGraph> Graph;

		TBitArray<> ValidEdges;
		TArray<double> LengthSquared;
		TArray<FVector> Positions;
		TArray<FVector> Directions;
		TSharedPtr<PCGExOctree::FItemOctree> Octree;

		double ToleranceSquared = 10;

		FIntersectionCache(const TSharedPtr<FGraph>& InGraph, const TSharedPtr<PCGExData::FPointIO>& InPointIO);

		bool InitProxy(const TSharedPtr<FEdgeProxy>& Edge, int32 Index) const;

	protected:
		double Tolerance = 10;
		void BuildCache();
	};

	class PCGEXGRAPHS_API FEdgeProxy : public TSharedFromThis<FEdgeProxy>
	{
	public:
		virtual ~FEdgeProxy() = default;
		int32 Index = -1;
		int32 Start = 0;
		int32 End = 0;
		FBox Box = FBox(NoInit);

		FEdgeProxy() = default;

		virtual void Init(const FEdge& InEdge, const FVector& InStart, const FVector& InEnd, double Tolerance);
		virtual bool IsEmpty() const { return true; }
	};

#pragma region Point Edge intersections

	struct PCGEXGRAPHS_API FPESplit
	{
		int32 Index = -1;
		double Time = -1;
		FVector ClosestPoint = FVector::ZeroVector;

		bool operator==(const FPESplit& Other) const { return Index == Other.Index; }
	};

	class PCGEXGRAPHS_API FPointEdgeProxy : public FEdgeProxy
	{
	public:
		TArray<FPESplit, TInlineAllocator<8>> CollinearPoints;

		virtual void Init(const FEdge& InEdge, const FVector& InStart, const FVector& InEnd, double Tolerance) override;
		bool FindSplit(int32 PointIndex, const TSharedPtr<FIntersectionCache>& Cache, FPESplit& OutSplit) const;
		void Add(const FPESplit& Split);

		virtual bool IsEmpty() const override { return CollinearPoints.IsEmpty(); }
	};

	class PCGEXGRAPHS_API FPointEdgeIntersections : public FIntersectionCache
	{
	public:
		const FPCGExPointEdgeIntersectionDetails* Details;
		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<FPointEdgeProxy>>> ScopedEdges;
		TArray<TSharedPtr<FPointEdgeProxy>> Edges;

		FPointEdgeIntersections(
			const TSharedPtr<FGraph>& InGraph,
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const FPCGExPointEdgeIntersectionDetails* InDetails);

		void Init(const TArray<PCGExMT::FScope>& Loops);
		void InsertEdges();
		void BlendIntersection(int32 Index, PCGExBlending::FMetadataBlender* Blender) const;

		~FPointEdgeIntersections() = default;
	};

	void FindCollinearNodes(
		const TSharedPtr<FPointEdgeIntersections>& InIntersections,
		const TSharedPtr<FPointEdgeProxy>& EdgeProxy);

	void FindCollinearNodes_NoSelfIntersections(
		const TSharedPtr<FPointEdgeIntersections>& InIntersections,
		const TSharedPtr<FPointEdgeProxy>& EdgeProxy);

#pragma endregion

#pragma region Edge Edge intersections

	struct PCGEXGRAPHS_API FEESplit
	{
		FEESplit() = default;

		int32 A = -1;
		int32 B = -1;
		double TimeA = -1;
		double TimeB = -1;
		FVector Center = FVector::ZeroVector;

		FORCEINLINE uint64 H64U() const { return PCGEx::H64U(A, B); }
	};

	struct PCGEXGRAPHS_API FEECrossing
	{
		int32 Index = -1;
		FEESplit Split;

		FEECrossing() = default;

		FORCEINLINE double GetTime(int32 EdgeIndex) const
		{
			return EdgeIndex == Split.A ? Split.TimeA : Split.TimeB;
		}

		bool operator==(const FEECrossing& Other) const { return Index == Other.Index; }
	};

	class PCGEXGRAPHS_API FEdgeEdgeProxy : public FEdgeProxy
	{
	public:
		TArray<FEECrossing> Crossings;

		bool FindSplit(const FEdge& OtherEdge, const TSharedPtr<FIntersectionCache>& Cache);
		virtual bool IsEmpty() const override { return Crossings.IsEmpty(); }
	};

	class PCGEXGRAPHS_API FEdgeEdgeIntersections : public FIntersectionCache
	{
	public:
		const FPCGExEdgeEdgeIntersectionDetails* Details;
		TSharedPtr<PCGExMT::TScopedArray<TSharedPtr<FEdgeEdgeProxy>>> ScopedEdges;

		TArray<FEECrossing> UniqueCrossings;
		TArray<TSharedPtr<FEdgeEdgeProxy>> Edges;

		FEdgeEdgeIntersections(
			const TSharedPtr<FGraph>& InGraph,
			const TSharedPtr<FUnionGraph>& InUnionGraph,
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const FPCGExEdgeEdgeIntersectionDetails* InDetails);

		void Init(const TArray<PCGExMT::FScope>& Loops);
		void Collapse(int32 InReserve);

		bool InsertNodes(int32 InReserve);
		void InsertEdges();

		void BlendIntersection(
			int32 Index,
			const TSharedRef<PCGExBlending::FMetadataBlender>& Blender,
			TArray<PCGEx::FOpStats>& Trackers) const;

		~FEdgeEdgeIntersections() = default;
	};

	void FindOverlappingEdges(
		const TSharedPtr<FEdgeEdgeIntersections>& InIntersections,
		const TSharedPtr<FEdgeEdgeProxy>& EdgeProxy);

	void FindOverlappingEdges_NoSelfIntersections(
		const TSharedPtr<FEdgeEdgeIntersections>& InIntersections,
		const TSharedPtr<FEdgeEdgeProxy>& EdgeProxy);

#pragma endregion
}