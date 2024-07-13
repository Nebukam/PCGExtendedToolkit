// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExAttributeHelpers.h"
#include "PCGExGraph.h"
#include "PCGExEdge.h"
#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataForward.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExIntersections.generated.h"

#define PCGEX_FOREACH_FIELD_INTERSECTION(MACRO)\
MACRO(IsIntersection, bool, false)\
MACRO(Normal, FVector, FVector::ZeroVector)\
MACRO(BoundIndex, int32, -1)\
MACRO(IsInside, bool, false)

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBoxIntersectionDetails
{
	GENERATED_BODY()

	/** If enabled, mark non-intersecting points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsIntersection = true;

	/** Name of the attribute to write point intersection boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteIsIntersection" ))
	FName IsIntersectionAttributeName = FName("IsIntersection");

	/** If enabled, mark non-intersecting points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** Name of the attribute to write point intersection normal to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteNormal" ))
	FName NormalAttributeName = FName("Normal");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteBoundIndex = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteBoundIndex" ))
	FName BoundIndexAttributeName = FName("BoundIndex");

	/** If enabled, mark points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsInside = true;

	/** Name of the attribute to write inside boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bWriteIsInside" ))
	FName IsInsideAttributeName = FName("IsInside");


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding", meta=(PCG_Overridable))
	FPCGExForwardDetails IntersectionForwarding;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding", meta=(PCG_Overridable))
	FPCGExForwardDetails InsideForwarding;

	bool Validate(const FPCGContext* InContext) const
	{
#define PCGEX_LOCAL_DETAIL_CHECK(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME) { PCGEX_VALIDATE_NAME_C(InContext, _NAME##AttributeName) }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_CHECK)
#undef PCGEX_LOCAL_DETAIL_CHECK

		return true;
	}

#define PCGEX_LOCAL_DETAIL_DECL(_NAME, _TYPE, _DEFAULT) PCGEx::TFAttributeWriter<_TYPE>* _NAME##Writer = nullptr;
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_DECL)
#undef PCGEX_LOCAL_DETAIL_DECL

	PCGExData::FDataForwardHandler* IntersectionForwardHandler = nullptr;
	PCGExData::FDataForwardHandler* InsideForwardHandler = nullptr;

	void Init(PCGExData::FFacade* PointDataFacade, PCGExData::FFacade* BoundsDataFacade)
	{
		PointDataFacade->Source->CreateOutKeys();

		IntersectionForwardHandler = IntersectionForwarding.TryGetHandler(BoundsDataFacade, PointDataFacade);
		InsideForwardHandler = InsideForwarding.TryGetHandler(BoundsDataFacade, PointDataFacade);

#define PCGEX_LOCAL_DETAIL_WRITER(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ _NAME##Writer = PointDataFacade->GetOrCreateWriter( _NAME##AttributeName, _DEFAULT, false, false); }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_WRITER)
#undef PCGEX_LOCAL_DETAIL_WRITER
	}

	void Cleanup()
	{
		PCGEX_DELETE(IntersectionForwardHandler)
		PCGEX_DELETE(InsideForwardHandler)
	}

	bool WillWriteAny() const
	{
#define PCGEX_LOCAL_DETAIL_WILL_WRITE(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ return true; }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_WILL_WRITE)
#undef PCGEX_LOCAL_DETAIL_WILL_WRITE

		return IntersectionForwarding.bEnabled || InsideForwarding.bEnabled;
	}

	void Mark(UPCGMetadata* Metadata) const
	{
#define PCGEX_LOCAL_DETAIL_MARK(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME) { PCGExData::WriteMark(Metadata, _NAME##AttributeName, _DEFAULT); }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_MARK)
#undef PCGEX_LOCAL_DETAIL_MARK
	}

	void SetIsInside(const int32 PointIndex, const bool bIsInside, const int32 BoundIndex) const
	{
		if (InsideForwardHandler && bIsInside) { InsideForwardHandler->Forward(PointIndex, BoundIndex); }
		if (IsInsideWriter) { IsInsideWriter->Values[PointIndex] = bIsInside; }
	}

	void SetIsInside(const int32 PointIndex, const bool bIsInside) const
	{
		if (IsInsideWriter) { IsInsideWriter->Values[PointIndex] = bIsInside; }
	}

	void SetIntersection(const int32 PointIndex, const FVector& Normal, const int32 BoundIndex) const
	{
		if (IntersectionForwardHandler) { IntersectionForwardHandler->Forward(BoundIndex, PointIndex); }

		if (IsIntersectionWriter) { IsIntersectionWriter->Values[PointIndex] = true; }
		if (NormalWriter) { NormalWriter->Values[PointIndex] = Normal; }
		if (BoundIndexWriter) { BoundIndexWriter->Values[PointIndex] = BoundIndex; }
	}
};

#undef PCGEX_FOREACH_FIELD_INTERSECTION

namespace PCGExGraph
{
#pragma region Compound Graph

	struct PCGEXTENDEDTOOLKIT_API FCompoundNode
	{
		const FPCGPoint Point;
		FVector Center;
		FBoxSphereBounds Bounds;
		int32 Index;

		TSet<int32> Adjacency;

		FCompoundNode(const FPCGPoint& InPoint, const FVector& InCenter, const int32 InIndex)
			: Point(InPoint),
			  Center(InCenter),
			  Index(InIndex)
		{
			Adjacency.Empty();
			Bounds = FBoxSphereBounds(InPoint.GetLocalBounds().TransformBy(InPoint.Transform));
		}

		~FCompoundNode()
		{
			Adjacency.Empty();
		}

		FVector UpdateCenter(const PCGExData::FIdxCompoundList* PointsCompounds, PCGExData::FPointIOCollection* IOGroup);
	};

	struct PCGEXTENDEDTOOLKIT_API FCompoundNodeSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FCompoundNode* InNode)
		{
			return InNode->Bounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FCompoundNode* A, const FCompoundNode* B)
		{
			return A == B;
		}

		FORCEINLINE static void ApplyOffset(FCompoundNode& InNode)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FCompoundNode* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FCompoundGraph
	{
		FVector CWTolerance;
		TMap<uint32, FCompoundNode*> GridTree;

		PCGExData::FIdxCompoundList* PointsCompounds = nullptr;
		PCGExData::FIdxCompoundList* EdgesCompounds = nullptr;
		TArray<FCompoundNode*> Nodes;
		TMap<uint64, FIndexedEdge> Edges;

		const FPCGExFuseDetails FuseDetails;
		EPCGExFuseMethod Precision;

		FBox Bounds;
		const bool bFusePoints = true;

		using NodeOctree = TOctree2<FCompoundNode*, FCompoundNodeSemantics>;
		NodeOctree* Octree = nullptr;

		mutable FRWLock OctreeLock;
		mutable FRWLock EdgesLock;

		explicit FCompoundGraph(const FPCGExFuseDetails& InFuseDetails, const FBox& InBounds, const bool FusePoints = true, const EPCGExFuseMethod InPrecision = EPCGExFuseMethod::Voxel)
			: FuseDetails(InFuseDetails),
			  Precision(InPrecision),
			  Bounds(InBounds),
			  bFusePoints(FusePoints)
		{
			Nodes.Empty();
			Edges.Empty();

			if (InFuseDetails.bComponentWiseTolerance) { FVector(1 / (InFuseDetails.Tolerances.X * 2), 1 / (InFuseDetails.Tolerances.Y * 2), 1 / (InFuseDetails.Tolerances.Z * 2)); }
			else { CWTolerance = FVector(1 / (InFuseDetails.Tolerance * 2)); }

			PointsCompounds = new PCGExData::FIdxCompoundList();
			EdgesCompounds = new PCGExData::FIdxCompoundList();

			if (InPrecision == EPCGExFuseMethod::Octree) { Octree = new NodeOctree(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10); }
		}

		~FCompoundGraph()
		{
			PCGEX_DELETE_TARRAY(Nodes)
			PCGEX_DELETE(PointsCompounds)
			PCGEX_DELETE(EdgesCompounds)
			PCGEX_DELETE(Octree)
			Edges.Empty();
		}

		int32 NumNodes() const { return PointsCompounds->Num(); }
		int32 NumEdges() const { return EdgesCompounds->Num(); }

		FCompoundNode* InsertPoint(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex);
		FCompoundNode* InsertPointUnsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex);
		PCGExData::FIdxCompound* InsertEdge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex,
		                                    const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex,
		                                    const int32 EdgeIOIndex = -1, const int32 EdgePointIndex = -1);
		PCGExData::FIdxCompound* InsertEdgeUnsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex,
		                                          const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex,
		                                          const int32 EdgeIOIndex = -1, const int32 EdgePointIndex = -1);
		void GetUniqueEdges(TArray<FUnsignedEdge>& OutEdges);
		void WriteMetadata(TMap<int32, FGraphNodeMetadata*>& OutMetadata);
	};

#pragma endregion

#pragma region Point Edge intersections

	struct PCGEXTENDEDTOOLKIT_API FPESplit
	{
		int32 NodeIndex = -1;
		double Time = -1;
		FVector ClosestPoint = FVector::ZeroVector;

		bool operator==(const FPESplit& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct PCGEXTENDEDTOOLKIT_API FPointEdgeProxy
	{
		int32 EdgeIndex = -1;
		TArray<FPESplit> CollinearPoints;

		double LengthSquared = -1;
		double ToleranceSquared = -1;
		FBox Box = FBox(NoInit);

		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;

		FPointEdgeProxy()
		{
		}

		explicit FPointEdgeProxy(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			Init(InEdgeIndex, InStart, InEnd, Tolerance);
		}

		void Init(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			CollinearPoints.Empty();

			Start = InStart;
			End = InEnd;

			EdgeIndex = InEdgeIndex;
			ToleranceSquared = Tolerance * Tolerance;

			Box = FBox(ForceInit);
			Box += Start;
			Box += End;
			Box = Box.ExpandBy(Tolerance);

			LengthSquared = FVector::DistSquared(Start, End);
		}

		~FPointEdgeProxy()
		{
			CollinearPoints.Empty();
		}

		FORCEINLINE bool FindSplit(const FVector& Position, FPESplit& OutSplit) const
		{
			const FVector ClosestPoint = FMath::ClosestPointOnSegment(Position, Start, End);

			if ((ClosestPoint - Start).IsNearlyZero() || (ClosestPoint - End).IsNearlyZero()) { return false; } // Overlap endpoint
			if (FVector::DistSquared(ClosestPoint, Position) >= ToleranceSquared) { return false; }             // Too far

			OutSplit.ClosestPoint = ClosestPoint;
			OutSplit.Time = (FVector::DistSquared(Start, ClosestPoint) / LengthSquared);
			return true;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPointEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		PCGExData::FPointIO* PointIO = nullptr;
		FGraph* Graph = nullptr;
		FCompoundGraph* CompoundGraph = nullptr;

		const FPCGExPointEdgeIntersectionDetails* Details;
		TArray<FPointEdgeProxy> Edges;

		FPointEdgeIntersections(
			FGraph* InGraph,
			FCompoundGraph* InCompoundGraph,
			PCGExData::FPointIO* InPointIO,
			const FPCGExPointEdgeIntersectionDetails* InDetails);

		void FindIntersections(FPCGExPointsProcessorContext* InContext);

		FORCEINLINE void Add(const int32 EdgeIndex, const FPESplit& Split)
		{
			FWriteScopeLock WriteLock(InsertionLock);
			Edges[EdgeIndex].CollinearPoints.AddUnique(Split);
		}

		void Insert();

		void BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const;

		~FPointEdgeIntersections()
		{
			Edges.Empty();
		}
	};

	static void FindCollinearNodes(
		FPointEdgeIntersections* InIntersections,
		const int32 EdgeIndex,
		const UPCGPointData* PointsData)
	{
		const TArray<FPCGPoint>& Points = PointsData->GetPoints();

		const FPointEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		const FIndexedEdge& IEdge = InIntersections->Graph->Edges[EdgeIndex];
		FPESplit Split = FPESplit{};

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = FGraphEdgeMetadata::GetRootIndex(Edge.EdgeIndex, InIntersections->Graph->EdgeMetadata);
			const TSet<int32>& RootIOIndices = InIntersections->CompoundGraph->EdgesCompounds->Compounds[RootIndex]->IOIndices;

			auto ProcessPointRef = [&](const FPCGPointRef& PointRef)
			{
				const ptrdiff_t PointIndex = PointRef.Point - Points.GetData();

				if (!Points.IsValidIndex(static_cast<int32>(PointIndex))) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Points[Node.PointIndex].Transform.GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (!Edge.FindSplit(Position, Split)) { return; }

				if (InIntersections->CompoundGraph->PointsCompounds->IOIndexOverlap(Node.NodeIndex, RootIOIndices)) { return; }

				Split.NodeIndex = Node.NodeIndex;
				InIntersections->Add(EdgeIndex, Split);
			};

			PointsData->GetOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
		else
		{
			auto ProcessPointRef = [&](const FPCGPointRef& PointRef)
			{
				const ptrdiff_t PointIndex = PointRef.Point - Points.GetData();

				if (!Points.IsValidIndex(static_cast<int32>(PointIndex))) { return; }
				const FNode& Node = InIntersections->Graph->Nodes[PointIndex];

				if (!Node.bValid) { return; }

				const FVector Position = Points[Node.PointIndex].Transform.GetLocation();

				if (!Edge.Box.IsInside(Position)) { return; }
				if (IEdge.Start == Node.PointIndex || IEdge.End == Node.PointIndex) { return; }
				if (Edge.FindSplit(Position, Split))
				{
					Split.NodeIndex = Node.NodeIndex;
					InIntersections->Add(EdgeIndex, Split);
				}
			};

			PointsData->GetOctree().FindElementsWithBoundsTest(Edge.Box, ProcessPointRef);
		}
	}

#pragma endregion

#pragma region Edge Edge intersections

	struct PCGEXTENDEDTOOLKIT_API FEESplit
	{
		double TimeA = -1;
		double TimeB = -1;
		FVector Center = FVector::ZeroVector;
	};

	struct PCGEXTENDEDTOOLKIT_API FEECrossing
	{
		int32 NodeIndex = -1;
		int32 EdgeA = -1;
		int32 EdgeB = -1;
		FEESplit Split;

		explicit FEECrossing(const FEESplit& InSplit)
			: Split(InSplit)
		{
		}

		FORCEINLINE double GetTime(const int32 EdgeIndex) const { return EdgeIndex == EdgeA ? Split.TimeA : Split.TimeB; }

		bool operator==(const FEECrossing& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeProxy
	{
		int32 EdgeIndex = -1;
		TArray<FEECrossing*> Intersections;

		double LengthSquared = -1;
		double ToleranceSquared = -1;
		FBox Box = FBox(NoInit);
		FBoxSphereBounds FSBounds = FBoxSphereBounds{};

		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;

		FEdgeEdgeProxy()
		{
		}

		explicit FEdgeEdgeProxy(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			Init(InEdgeIndex, InStart, InEnd, Tolerance);
		}

		void Init(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance)
		{
			Intersections.Empty();

			Start = InStart;
			End = InEnd;

			EdgeIndex = InEdgeIndex;
			ToleranceSquared = Tolerance * Tolerance;

			Box = FBox(ForceInit);
			Box += Start;
			Box += End;
			Box = Box.ExpandBy(Tolerance);

			LengthSquared = FVector::DistSquared(Start, End);
			FSBounds = Box;
		}

		~FEdgeEdgeProxy()
		{
			Intersections.Empty();
		}

		FORCEINLINE bool FindSplit(const FEdgeEdgeProxy& OtherEdge, FEESplit& OutSplit) const
		{
			if (!Box.Intersect(OtherEdge.Box) || Start == OtherEdge.Start || Start == OtherEdge.End ||
				End == OtherEdge.End || End == OtherEdge.Start) { return false; }

			// TODO: Check directions/dot

			FVector A;
			FVector B;
			FMath::SegmentDistToSegment(
				Start, End,
				OtherEdge.Start, OtherEdge.End,
				A, B);

			if (FVector::DistSquared(A, B) >= ToleranceSquared) { return false; }

			OutSplit.Center = FMath::Lerp(A, B, 0.5);
			OutSplit.TimeA = FVector::DistSquared(Start, A) / LengthSquared;
			OutSplit.TimeB = FVector::DistSquared(OtherEdge.Start, B) / OtherEdge.LengthSquared;

			return true;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeProxySemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FEdgeEdgeProxy* InEdge)
		{
			return InEdge->FSBounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FEdgeEdgeProxy* A, const FEdgeEdgeProxy* B)
		{
			return A == B;
		}

		FORCEINLINE static void ApplyOffset(FEdgeEdgeProxy& InEdge)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FEdgeEdgeProxy* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		PCGExData::FPointIO* PointIO = nullptr;
		FGraph* Graph = nullptr;
		FCompoundGraph* CompoundGraph = nullptr;

		const FPCGExEdgeEdgeIntersectionDetails* Details;

		TArray<FEECrossing*> Crossings;
		TArray<FEdgeEdgeProxy> Edges;
		TSet<uint64> CheckedPairs;

		using TEdgeOctree = TOctree2<FEdgeEdgeProxy*, FEdgeEdgeProxySemantics>;
		mutable TEdgeOctree Octree;

		FEdgeEdgeIntersections(
			FGraph* InGraph,
			FCompoundGraph* InCompoundGraph,
			PCGExData::FPointIO* InPointIO,
			const FPCGExEdgeEdgeIntersectionDetails* InDetails);

		void FindIntersections(FPCGExPointsProcessorContext* InContext);

		FORCEINLINE void Add(const int32 EdgeIndex, const int32 OtherEdgeIndex, const FEESplit& Split)
		{
			FWriteScopeLock WriteLock(InsertionLock);

			CheckedPairs.Add(PCGEx::H64U(EdgeIndex, OtherEdgeIndex));

			FEECrossing* OutSplit = new FEECrossing(Split);

			OutSplit->NodeIndex = Crossings.Add(OutSplit) + Graph->Nodes.Num();
			OutSplit->EdgeA = FMath::Min(EdgeIndex, OtherEdgeIndex);
			OutSplit->EdgeB = FMath::Max(EdgeIndex, OtherEdgeIndex);

			Edges[EdgeIndex].Intersections.AddUnique(OutSplit);
			Edges[OtherEdgeIndex].Intersections.AddUnique(OutSplit);
		}

		void Insert();

		void BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const;

		~FEdgeEdgeIntersections()
		{
			CheckedPairs.Empty();
			Edges.Empty();
			PCGEX_DELETE_TARRAY(Crossings)
		}
	};

	static void FindOverlappingEdges(
		FEdgeEdgeIntersections* InIntersections,
		const int32 EdgeIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindOverlappingEdges);

		const FEdgeEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		FEESplit Split = FEESplit{};

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = FGraphEdgeMetadata::GetRootIndex(Edge.EdgeIndex, InIntersections->Graph->EdgeMetadata);
			const TSet<int32>& RootIOIndices = InIntersections->CompoundGraph->EdgesCompounds->Compounds[RootIndex]->IOIndices;

			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }

				{
					FReadScopeLock ReadLock(InIntersections->InsertionLock);
					if (InIntersections->CheckedPairs.Contains(PCGEx::H64U(EdgeIndex, OtherEdge.EdgeIndex))) { return; }
				}

				if (!Edge.FindSplit(OtherEdge, Split)) { return; }

				// Check overlap last as it's the most expensive op
				if (InIntersections->CompoundGraph->PointsCompounds->IOIndexOverlap(
					FGraphEdgeMetadata::GetRootIndex(OtherEdge.EdgeIndex, InIntersections->Graph->EdgeMetadata),
					RootIOIndices)) { return; }

				InIntersections->Add(EdgeIndex, OtherEdge.EdgeIndex, Split);
			};

			InIntersections->Octree.FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}
		else
		{
			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }

				{
					FReadScopeLock ReadLock(InIntersections->InsertionLock);
					if (InIntersections->CheckedPairs.Contains(PCGEx::H64U(EdgeIndex, OtherEdge.EdgeIndex))) { return; }
				}

				if (Edge.FindSplit(OtherEdge, Split))
				{
					InIntersections->Add(EdgeIndex, OtherEdge.EdgeIndex, Split);
				}
			};

			InIntersections->Octree.FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}
	}

#pragma endregion
}

namespace PCGExGraphTask
{
#pragma region Intersections tasks

	class PCGEXTENDEDTOOLKIT_API FFindPointEdgeIntersections final : public PCGExMT::FPCGExTask
	{
	public:
		FFindPointEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                            PCGExGraph::FPointEdgeIntersections* InIntersectionList)
			: FPCGExTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FPointEdgeIntersections* IntersectionList = nullptr;
		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FInsertPointEdgeIntersections final : public PCGExMT::FPCGExTask
	{
	public:
		FInsertPointEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                              PCGExGraph::FPointEdgeIntersections* InIntersectionList)
			: FPCGExTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FPointEdgeIntersections* IntersectionList = nullptr;

		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FFindEdgeEdgeIntersections final : public PCGExMT::FPCGExTask
	{
	public:
		FFindEdgeEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                           PCGExGraph::FEdgeEdgeIntersections* InIntersectionList)
			: FPCGExTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FEdgeEdgeIntersections* IntersectionList = nullptr;
		virtual bool ExecuteTask() override;
	};

	class PCGEXTENDEDTOOLKIT_API FInsertEdgeEdgeIntersections final : public PCGExMT::FPCGExTask
	{
	public:
		FInsertEdgeEdgeIntersections(PCGExData::FPointIO* InPointIO,
		                             PCGExGraph::FEdgeEdgeIntersections* InIntersectionList, TMap<int32, PCGExGraph::FGraphNodeMetadata*>* InOutMetadata)
			: FPCGExTask(InPointIO),
			  IntersectionList(InIntersectionList)
		{
		}

		PCGExGraph::FEdgeEdgeIntersections* IntersectionList = nullptr;
		TMap<int32, PCGExGraph::FGraphNodeMetadata*>* OutMetadata = nullptr;

		virtual bool ExecuteTask() override;
	};

#pragma endregion
}
