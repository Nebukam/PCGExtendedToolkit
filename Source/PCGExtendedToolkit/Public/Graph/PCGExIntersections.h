// Copyright 2025 Timothé Lapetite and contributors
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
struct PCGEXTENDEDTOOLKIT_API FPCGExBoxIntersectionDetails
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
	double InsideExpansion = -1e-4;

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

#define PCGEX_LOCAL_DETAIL_WRITER(_NAME, _TYPE, _DEFAULT) if (bWrite##_NAME){ _NAME##Writer = PointDataFacade->GetWritable( _NAME##AttributeName, _DEFAULT, true, PCGExData::EBufferInit::Inherit); }
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

	class PCGEXTENDEDTOOLKIT_API FUnionNode : public TSharedFromThis<FUnionNode>
	{
	protected:
		mutable FRWLock AdjacencyLock;

	public:
		const PCGExData::FConstPoint Point;
		FVector Center;
		FBoxSphereBounds Bounds;
		int32 Index;

		TSet<int32> Adjacency;

		FUnionNode(const PCGExData::FConstPoint& InPoint, const FVector& InCenter, const int32 InIndex);
		~FUnionNode() = default;

		FVector UpdateCenter(const TSharedPtr<PCGExData::FUnionMetadata>& InUnionMetadata, const TSharedPtr<PCGExData::FPointIOCollection>& IOGroup);

		void Add(const int32 InAdjacency);
	};

	PCGEX_OCTREE_SEMANTICS(FUnionNode, { return Element->Bounds;}, { return A->Index == B->Index; })

	class PCGEXTENDEDTOOLKIT_API FUnionGraph : public TSharedFromThis<FUnionGraph>
	{
	public:
		TMap<uint32, TSharedPtr<FUnionNode>> GridTree;

		TSharedPtr<PCGExData::FUnionMetadata> NodesUnion;
		TSharedPtr<PCGExData::FUnionMetadata> EdgesUnion;
		TArray<TSharedPtr<FUnionNode>> Nodes;
		TMap<uint64, FEdge> Edges;

		FPCGExFuseDetails FuseDetails;

		FBox Bounds;

		TUniquePtr<FUnionNodeOctree> Octree;

		mutable FRWLock UnionLock;
		mutable FRWLock EdgesLock;

		explicit FUnionGraph(const FPCGExFuseDetails& InFuseDetails, const FBox& InBounds);

		~FUnionGraph() = default;

		bool Init(FPCGExContext* InContext);
		bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InUniqueSourceFacade, const bool SupportScopedGet);

		int32 NumNodes() const { return NodesUnion->Num(); }
		int32 NumEdges() const { return EdgesUnion->Num(); }

		TSharedPtr<FUnionNode> InsertPoint(const PCGExData::FConstPoint& Point);
		TSharedPtr<FUnionNode> InsertPoint_Unsafe(const PCGExData::FConstPoint& Point);
		TSharedPtr<PCGExData::FUnionData> InsertEdge(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge = PCGExData::NONE_ConstPoint);
		TSharedPtr<PCGExData::FUnionData> InsertEdge_Unsafe(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge = PCGExData::NONE_ConstPoint);
		void GetUniqueEdges(TSet<uint64>& OutEdges);
		void GetUniqueEdges(TArray<FEdge>& OutEdges);
		void WriteNodeMetadata(const TSharedPtr<FGraph>& InGraph) const;
		void WriteEdgeMetadata(const TSharedPtr<FGraph>& InGraph) const;
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
			const double Tolerance);

		void Init(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance);

		~FPointEdgeProxy()
		{
			CollinearPoints.Empty();
		}

		bool FindSplit(const FVector& Position, FPESplit& OutSplit) const;
	};

	struct PCGEXTENDEDTOOLKIT_API FPointEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FGraph> Graph;

		const FPCGExPointEdgeIntersectionDetails* Details;
		TArray<FPointEdgeProxy> Edges;

		FPointEdgeIntersections(
			const TSharedPtr<FGraph>& InGraph,
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const FPCGExPointEdgeIntersectionDetails* InDetails);

		void Add(const int32 EdgeIndex, const FPESplit& Split);
		void Insert();
		void BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const;

		~FPointEdgeIntersections() = default;
	};

	void FindCollinearNodes(
		const TSharedPtr<FPointEdgeIntersections>& InIntersections,
		const int32 EdgeIndex,
		const UPCGBasePointData* PointsData);

#pragma endregion

#pragma region Edge Edge intersections

	struct PCGEXTENDEDTOOLKIT_API FEESplit
	{
		int32 A = -1;
		int32 B = -1;
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

		explicit FEECrossing(const FEESplit& InSplit);

		FORCEINLINE double GetTime(const int32 EdgeIndex) const { return EdgeIndex == EdgeA ? Split.TimeA : Split.TimeB; }

		bool operator==(const FEECrossing& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeProxy
	{
		int32 EdgeIndex = -1;
		TArray<int32> Intersections;

		double LengthSquared = -1;
		double ToleranceSquared = -1;
		FBox Box = FBox(NoInit);
		FBoxSphereBounds Bounds = FBoxSphereBounds{};

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
			const double Tolerance);


		void Init(
			const int32 InEdgeIndex,
			const FVector& InStart,
			const FVector& InEnd,
			const double Tolerance);

		~FEdgeEdgeProxy() = default;

		bool FindSplit(const FEdgeEdgeProxy& OtherEdge, TArray<FEESplit>& OutSplits) const;
	};

	PCGEX_OCTREE_SEMANTICS(FEdgeEdgeProxy, { return Element->Bounds;}, { return A == B; })

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeIntersections
	{
		mutable FRWLock InsertionLock;
		const TSharedPtr<PCGExData::FPointIO> PointIO;
		TSharedPtr<FGraph> Graph;

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

		bool AlreadyChecked(const uint64 Key) const;

		void Add_Unsafe(const FEESplit& Split);
		void BatchAdd(TArray<FEESplit>& Splits, const int32 A);

		bool InsertNodes() const;
		void InsertEdges();

		void BlendIntersection(const int32 Index, const TSharedRef<PCGExDataBlending::FMetadataBlender>& Blender, TArray<PCGEx::FOpStats>& Trackers) const;

		~FEdgeEdgeIntersections() = default;
	};

	void FindOverlappingEdges(
		const TSharedRef<FEdgeEdgeIntersections>& InIntersections,
		const int32 EdgeIndex);

#pragma endregion
}
