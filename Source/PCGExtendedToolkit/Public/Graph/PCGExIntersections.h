// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExOctree.h"
#include "Data/PCGExDataForward.h"
#include "Graph/PCGExEdge.h"
#include "Details/PCGExDetailsFusing.h"

#include "PCGExIntersections.generated.h"

#define PCGEX_FOREACH_FIELD_INTERSECTION(MACRO)\
MACRO(IsIntersection, bool, false)\
MACRO(CutType, int32, CutTypeValueMapping[EPCGExCutType::Undefined])\
MACRO(Normal, FVector, FVector::ZeroVector)\
MACRO(BoundIndex, int32, -1)

struct FPCGExEdgeEdgeIntersectionDetails;
struct FPCGExPointEdgeIntersectionDetails;

namespace PCGExDataBlending
{
	class FMetadataBlender;
}

namespace PCGExData
{
	class FUnionMetadata;
	class IUnionData;
}

enum class EPCGExCutType : uint8;

namespace PCGExSampling
{
	class FTargetsHandler;
}

namespace PCGExGeo
{
	struct FCut;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBoxIntersectionDetails
{
	GENERATED_BODY()

	FPCGExBoxIntersectionDetails();

	/** Bounds type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** If enabled, flag newly created intersection points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteIsIntersection = true;

	/** Name of the attribute to write point intersection boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="IsIntersection", PCG_Overridable, EditCondition="bWriteIsIntersection" ))
	FName IsIntersectionAttributeName = FName("IsIntersection");

	/** If enabled, mark non-intersecting points inside the volume with a boolean value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCutType = true;

	/** Name of the attribute to write point toward inside boolean to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="CutType", PCG_Overridable, EditCondition="bWriteCutType" ))
	FName CutTypeAttributeName = FName("CutType");

	/** Pick which value will be written for each cut type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings", EditFixedSize, meta=( ReadOnlyKeys, DisplayName=" └─ Mapping", EditCondition="bWriteCutType", HideEditConditionToggle))
	TMap<EPCGExCutType, int32> CutTypeValueMapping;

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

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Forwarding", meta=(PCG_Overridable))
	FPCGExForwardDetails IntersectionForwarding;

	bool Validate(const FPCGContext* InContext) const;

#define PCGEX_LOCAL_DETAIL_DECL(_NAME, _TYPE, _DEFAULT) TSharedPtr<PCGExData::TBuffer<_TYPE>> _NAME##Writer;
	PCGEX_FOREACH_FIELD_INTERSECTION(PCGEX_LOCAL_DETAIL_DECL)
#undef PCGEX_LOCAL_DETAIL_DECL

	void Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade, const TSharedPtr<PCGExSampling::FTargetsHandler>& TargetsHandler);

	bool WillWriteAny() const;

	void Mark(const TSharedRef<PCGExData::FPointIO>& InPointIO) const;
	void SetIntersection(const int32 PointIndex, const PCGExGeo::FCut& InCut) const;

private:
	TArray<TSharedPtr<PCGExData::FDataForwardHandler>> IntersectionForwardHandlers;
};

namespace PCGExGraph
{
	class FGraph;
	struct FEdge;
	
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

		TSet<int32, DefaultKeyFuncs<int32>, InlineSparseAllocator> Adjacency;

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

		int32 NumNodes() const;
		int32 NumEdges() const;

		TSharedPtr<FUnionNode> InsertPoint(const PCGExData::FConstPoint& Point);
		TSharedPtr<FUnionNode> InsertPoint_Unsafe(const PCGExData::FConstPoint& Point);
		TSharedPtr<PCGExData::IUnionData> InsertEdge(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge = PCGExData::NONE_ConstPoint);
		TSharedPtr<PCGExData::IUnionData> InsertEdge_Unsafe(const PCGExData::FConstPoint& From, const PCGExData::FConstPoint& To, const PCGExData::FConstPoint& Edge = PCGExData::NONE_ConstPoint);
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
		FEESplit Split;

		explicit FEECrossing(const FEESplit& InSplit);

		FORCEINLINE double GetTime(const int32 EdgeIndex) const { return EdgeIndex == Split.A ? Split.TimeA : Split.TimeB; }

		bool operator==(const FEECrossing& Other) const { return NodeIndex == Other.NodeIndex; }
	};

	struct PCGEXTENDEDTOOLKIT_API FEdgeEdgeProxy
	{
		bool bIsValid = false;
		int32 EdgeIndex = -1;
		TArray<int32> Intersections;

		double LengthSquared = -1;
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

		bool FindSplit(const FEdgeEdgeProxy& OtherEdge, TArray<FEESplit>& OutSplits, const FPCGExEdgeEdgeIntersectionDetails* InIntersectionDetails) const;
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
