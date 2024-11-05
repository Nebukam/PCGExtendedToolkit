// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

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
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBoxIntersectionDetails
{
	GENERATED_BODY()

	/** Bounds type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** If enabled, mark non-intersecting points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsIntersection = true;

	/** Name of the attribute to write point intersection boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="IsIntersection", PCG_Overridable, EditCondition="bWriteIsIntersection" ))
	FName IsIntersectionAttributeName = FName("IsIntersection");

	/** If enabled, mark non-intersecting points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNormal = false;

	/** Name of the attribute to write point intersection normal to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Normal", PCG_Overridable, EditCondition="bWriteNormal" ))
	FName NormalAttributeName = FName("Normal");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteBoundIndex = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="BoundIndex", PCG_Overridable, EditCondition="bWriteBoundIndex" ))
	FName BoundIndexAttributeName = FName("BoundIndex");

	/** If enabled, mark points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsInside = true;

	/** Name of the attribute to write inside boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="IsInside", PCG_Overridable, EditCondition="bWriteIsInside" ))
	FName IsInsideAttributeName = FName("IsInside");


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding", meta=(PCG_Overridable))
	FPCGExForwardDetails IntersectionForwarding;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding", meta=(PCG_Overridable))
	FPCGExForwardDetails InsideForwarding;

	/** Epsilon value used to expand the box when testing if IsInside. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double InsideEpsilon = 1e-4;

	bool Validate(const FPCGContext* InContext) const
	{
#define PCGEX_LOCAL_DETAIL_CHECK(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME) { PCGEX_VALIDATE_NAME_C(InContext, _NAME##AttributeName) }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_CHECK)
#undef PCGEX_LOCAL_DETAIL_CHECK

		return true;
	}

#define PCGEX_LOCAL_DETAIL_DECL(_NAME, _TYPE, _DEFAULT) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Writer;
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_DECL)
#undef PCGEX_LOCAL_DETAIL_DECL

	TSharedPtr<PCGExData::FDataForwardHandler> IntersectionForwardHandler;
	TSharedPtr<PCGExData::FDataForwardHandler> InsideForwardHandler;

	void Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade, const TSharedPtr<PCGExData::FFacade>& BoundsDataFacade)
	{
		IntersectionForwardHandler = IntersectionForwarding.TryGetHandler(BoundsDataFacade, PointDataFacade);
		InsideForwardHandler = InsideForwarding.TryGetHandler(BoundsDataFacade, PointDataFacade);

#define PCGEX_LOCAL_DETAIL_WRITER(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ _NAME##Writer = PointDataFacade->GetWritable( _NAME##AttributeName, _DEFAULT, false, false); }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_WRITER)
#undef PCGEX_LOCAL_DETAIL_WRITER
	}

	bool WillWriteAny() const
	{
#define PCGEX_LOCAL_DETAIL_WILL_WRITE(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ return true; }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_WILL_WRITE)
#undef PCGEX_LOCAL_DETAIL_WILL_WRITE

		return IntersectionForwarding.bEnabled || InsideForwarding.bEnabled;
	}

	void Mark(const TSharedRef<PCGExData::FPointIO>& InPointIO) const
	{
#define PCGEX_LOCAL_DETAIL_MARK(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME) { PCGExData::WriteMark(InPointIO, _NAME##AttributeName, _DEFAULT); }
		PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_MARK)
#undef PCGEX_LOCAL_DETAIL_MARK
	}

	void SetIsInside(const int32 PointIndex, const bool bIsInside, const int32 BoundIndex) const
	{
		if (InsideForwardHandler && bIsInside) { InsideForwardHandler->Forward(BoundIndex, PointIndex); }
		if (IsInsideWriter) { IsInsideWriter->GetMutable(PointIndex) = bIsInside; }
	}

	void SetIsInside(const int32 PointIndex, const bool bIsInside) const
	{
		if (IsInsideWriter) { IsInsideWriter->GetMutable(PointIndex) = bIsInside; }
	}

	void SetIntersection(const int32 PointIndex, const FVector& Normal, const int32 BoundIndex) const
	{
		if (IntersectionForwardHandler) { IntersectionForwardHandler->Forward(BoundIndex, PointIndex); }

		if (IsIntersectionWriter) { IsIntersectionWriter->GetMutable(PointIndex) = true; }
		if (NormalWriter) { NormalWriter->GetMutable(PointIndex) = Normal; }
		if (BoundIndexWriter) { BoundIndexWriter->GetMutable(PointIndex) = BoundIndex; }
	}
};

#undef PCGEX_FOREACH_FIELD_INTERSECTION

namespace PCGExGraph
{
#pragma region Compound Graph

	struct /*PCGEXTENDEDTOOLKIT_API*/ FUnionNode
	{
	protected:
		mutable FRWLock AdjacencyLock;

	public:
		const FPCGPoint Point;
		FVector Center;
		FBoxSphereBounds BSB;
		int32 Index;

		TSet<int32> Adjacency;

		FUnionNode(const FPCGPoint& InPoint, const FVector& InCenter, const int32 InIndex)
			: Point(InPoint),
			  Center(InCenter),
			  Index(InIndex)
		{
			Adjacency.Empty();
			BSB = FBoxSphereBounds(InPoint.GetLocalBounds().TransformBy(InPoint.Transform));
		}

		~FUnionNode()
		{
			Adjacency.Empty();
		}

		FVector UpdateCenter(const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup);

		FORCEINLINE void Add(const int32 InAdjacency)
		{
			FWriteScopeLock WriteScopeLock(AdjacencyLock);
			Adjacency.Add(InAdjacency);
		}
	};

	PCGEX_OCTREE_SEMANTICS(FUnionNode, { return Element->BSB;}, { return A->Index == B->Index; })

	struct /*PCGEXTENDEDTOOLKIT_API*/ FUnionGraph
	{
		TMap<uint32, FUnionNode*> GridTree;

		TSharedPtr<PCGExData::FUnionMetadata> PointsUnion;
		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;
		TArray<FUnionNode*> Nodes;
		TMap<uint64, FIndexedEdge> Edges;

		FPCGExFuseDetails FuseDetails;

		FBox Bounds;

		TUniquePtr<FUnionNodeOctree> Octree;

		mutable FRWLock UnionLock;
		mutable FRWLock EdgesLock;

		explicit FUnionGraph(const FPCGExFuseDetails& InFuseDetails, const FBox& InBounds)
			: FuseDetails(InFuseDetails),
			  Bounds(InBounds)
		{
			Nodes.Empty();
			Edges.Empty();

			FuseDetails.Init();

			PointsUnion = MakeShared<PCGExData::FUnionMetadata>();
			EdgesUnion = MakeShared<PCGExData::FUnionMetadata>();

			if (InFuseDetails.FuseMethod == EPCGExFuseMethod::Octree) { Octree = MakeUnique<FUnionNodeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10); }
		}

		~FUnionGraph()
		{
			for (const FUnionNode* Node : Nodes) { delete Node; }
		}

		int32 NumNodes() const { return PointsUnion->Num(); }
		int32 NumEdges() const { return EdgesUnion->Num(); }

		FUnionNode* InsertPoint(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex);
		FUnionNode* InsertPointUnsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex);
		PCGExData::FUnionData* InsertEdge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex,
		                                  const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex,
		                                  const int32 EdgeIOIndex = -1, const int32 EdgePointIndex = -1);
		PCGExData::FUnionData* InsertEdgeUnsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex,
		                                        const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex,
		                                        const int32 EdgeIOIndex = -1, const int32 EdgePointIndex = -1);
		void GetUniqueEdges(TSet<uint64>& OutEdges);
		void GetUniqueEdges(TArray<FIndexedEdge>& OutEdges);
		void WriteNodeMetadata(const TSharedPtr<FGraph>& InGraph) const;
		void WriteEdgeMetadata(const TSharedPtr<FGraph>& InGraph) const;
	};

#pragma endregion

#pragma region Point Edge intersections

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPESplit
	{
		int32 NodeIndex = -1;
		double Time = -1;
		FVector ClosestPoint = FVector::ZeroVector;

		bool operator==(const FPESplit& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointEdgeProxy
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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPointEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FGraph> Graph;
		TSharedPtr<FUnionGraph> UnionGraph;

		const FPCGExPointEdgeIntersectionDetails* Details;
		TArray<FPointEdgeProxy> Edges;

		FPointEdgeIntersections(
			const TSharedPtr<FGraph>& InGraph,
			const TSharedPtr<FUnionGraph>& InUnionGraph,
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const FPCGExPointEdgeIntersectionDetails* InDetails);

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
		const TSharedPtr<FPointEdgeIntersections>& InIntersections,
		const int32 EdgeIndex,
		const UPCGPointData* PointsData)
	{
		const TArray<FPCGPoint>& Points = PointsData->GetPoints();

		const FPointEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		const FIndexedEdge& IEdge = InIntersections->Graph->Edges[EdgeIndex];
		FPESplit Split = FPESplit{};

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = InIntersections->Graph->FindEdgeMetadataUnsafe(Edge.EdgeIndex)->RootIndex;
			const TSet<int32>& RootIOIndices = InIntersections->UnionGraph->EdgesUnion->Entries[RootIndex]->IOIndices;

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

				if (InIntersections->UnionGraph->PointsUnion->IOIndexOverlap(Node.NodeIndex, RootIOIndices)) { return; }

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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FEESplit
	{
		int32 A = -1;
		int32 B = -1;
		double TimeA = -1;
		double TimeB = -1;
		FVector Center = FVector::ZeroVector;
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FEECrossing
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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FEdgeEdgeProxy
	{
		int32 EdgeIndex = -1;
		TArray<int32> Intersections;

		double LengthSquared = -1;
		double ToleranceSquared = -1;
		FBox Box = FBox(NoInit);
		FBoxSphereBounds BSB = FBoxSphereBounds{};

		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Direction = FVector::ZeroVector;

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
			BSB = Box;

			Direction = (Start - End).GetSafeNormal();
		}

		~FEdgeEdgeProxy()
		{
			Intersections.Empty();
		}

		FORCEINLINE bool FindSplit(const FEdgeEdgeProxy& OtherEdge, TArray<FEESplit>& OutSplits) const
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

			FEESplit& NewSplit = OutSplits.Emplace_GetRef();
			NewSplit.A = EdgeIndex;
			NewSplit.B = OtherEdge.EdgeIndex;
			NewSplit.Center = FMath::Lerp(A, B, 0.5);
			NewSplit.TimeA = FVector::DistSquared(Start, A) / LengthSquared;
			NewSplit.TimeB = FVector::DistSquared(OtherEdge.Start, B) / OtherEdge.LengthSquared;

			return true;
		}
	};

	PCGEX_OCTREE_SEMANTICS(FEdgeEdgeProxy, { return Element->BSB;}, { return A == B; })

	struct /*PCGEXTENDEDTOOLKIT_API*/ FEdgeEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FGraph> Graph;
		TSharedPtr<FUnionGraph> UnionGraph;

		const FPCGExEdgeEdgeIntersectionDetails* Details;

		TArray<FEECrossing> Crossings;
		TArray<FEdgeEdgeProxy> Edges;
		TSet<uint64> CheckedPairs;

		TUniquePtr<FEdgeEdgeProxyOctree> Octree;

		FEdgeEdgeIntersections(
			const TSharedPtr<FGraph>& InGraph,
			const TSharedPtr<FUnionGraph>& InUnionGraph,
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const FPCGExEdgeEdgeIntersectionDetails* InDetails);

		FORCEINLINE bool AlreadyChecked(const uint64 Key) const
		{
			FReadScopeLock ReadLock(InsertionLock);
			return CheckedPairs.Contains(Key);
		}

		FORCEINLINE void AddUnsafe(const FEESplit& Split)
		{
			bool bAlreadySet = false;
			CheckedPairs.Add(PCGEx::H64U(Split.A, Split.B), &bAlreadySet);

			if (bAlreadySet) { return; }

			FEECrossing& OutSplit = Crossings.Emplace_GetRef(Split);
			OutSplit.NodeIndex = Graph->Nodes.Num() + Crossings.Num() - 1;

			if (Split.A < Split.B)
			{
				OutSplit.EdgeA = Split.A;
				OutSplit.EdgeB = Split.B;
			}
			else
			{
				OutSplit.EdgeA = Split.B;
				OutSplit.EdgeB = Split.A;
			}

			Edges[Split.A].Intersections.AddUnique(Crossings.Num() - 1);
			Edges[Split.B].Intersections.AddUnique(Crossings.Num() - 1);
		}

		FORCEINLINE void BatchAdd(TArray<FEESplit>& Splits, const int32 A)
		{
			FWriteScopeLock WriteLock(InsertionLock);
			for (const FEESplit& Split : Splits) { AddUnsafe(Split); }
		}

		bool InsertNodes() const;
		void InsertEdges();

		void BlendIntersection(const int32 Index, const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender) const;

		~FEdgeEdgeIntersections()
		{
		}
	};

	static void FindOverlappingEdges(
		const TSharedRef<FEdgeEdgeIntersections>& InIntersections,
		const int32 EdgeIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindOverlappingEdges);

		const FEdgeEdgeProxy& Edge = InIntersections->Edges[EdgeIndex];
		TArray<FEESplit> OutSplits;

		if (!InIntersections->Details->bEnableSelfIntersection)
		{
			const int32 RootIndex = InIntersections->Graph->FindEdgeMetadataUnsafe(Edge.EdgeIndex)->RootIndex;
			const TSet<int32>& RootIOIndices = InIntersections->UnionGraph->EdgesUnion->Entries[RootIndex]->IOIndices;

			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }
				if (InIntersections->Details->bUseMinAngle || InIntersections->Details->bUseMaxAngle)
				{
					if (!InIntersections->Details->CheckDot(FMath::Abs(FVector::DotProduct(Edge.Direction, OtherEdge.Direction)))) { return; }
				}

				// Check overlap last as it's the most expensive op
				if (InIntersections->UnionGraph->EdgesUnion->IOIndexOverlap(InIntersections->Graph->FindEdgeMetadataUnsafe(OtherEdge.EdgeIndex)->RootIndex, RootIOIndices)) { return; }

				if (!Edge.FindSplit(OtherEdge, OutSplits))
				{
				}
			};

			InIntersections->Octree->FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}
		else
		{
			auto ProcessEdge = [&](const FEdgeEdgeProxy* Proxy)
			{
				const FEdgeEdgeProxy& OtherEdge = *Proxy;

				if (OtherEdge.EdgeIndex == -1 || &Edge == &OtherEdge) { return; }
				if (!Edge.Box.Intersect(OtherEdge.Box)) { return; }
				if (!Edge.FindSplit(OtherEdge, OutSplits))
				{
				}
			};

			InIntersections->Octree->FindElementsWithBoundsTest(Edge.Box, ProcessEdge);
		}

		InIntersections->BatchAdd(OutSplits, EdgeIndex);
	}

#pragma endregion
}
