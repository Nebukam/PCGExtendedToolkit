// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

#include "Core/PCGExContext.h"
#include "PCGExRandom.h"
#include "PCGExMT.h"
#include "Data/PCGExData.h"
#include "PCGExPointsProcessor.h"
#include "PCGExSortHelpers.h"
#include "Details/PCGExDetailsIntersection.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Metadata/PCGMetadata.h"

#include "Graph/PCGExCluster.h"
#include "Graph/Data/PCGExClusterData.h"

#include "Async/ParallelFor.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExUnionData.h"
#include "Details/PCGExDetailsDistances.h"

bool PCGExGraph::BuildIndexedEdges(const TSharedPtr<PCGExData::FPointIO>& EdgeIO, const TMap<uint32, int32>& EndpointsLookup, TArray<FEdge>& OutEdges, const bool bStopOnError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-Vanilla);

	const TUniquePtr<PCGExData::TArrayBuffer<int64>> EndpointsBuffer = MakeUnique<PCGExData::TArrayBuffer<int64>>(EdgeIO.ToSharedRef(), Attr_PCGExEdgeIdx);
	if (!EndpointsBuffer->InitForRead()) { return false; }

	const TArray<int64>& Endpoints = *EndpointsBuffer->GetInValues().Get();
	const int32 EdgeIOIndex = EdgeIO->IOIndex;

	int8 bValid = 1;
	const int32 NumEdges = EdgeIO->GetNum();

	PCGExArrayHelpers::InitArray(OutEdges, NumEdges);

	if (!bStopOnError)
	{
		int32 EdgeIndex = 0;

		for (int i = 0; i < NumEdges; i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr)) { continue; }

			OutEdges[EdgeIndex] = FEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
			EdgeIndex++;
		}

		PCGExArrayHelpers::InitArray(OutEdges, EdgeIndex);
	}
	else
	{
		PCGEX_PARALLEL_FOR_RET(
			NumEdges,
			true,
			if (!bValid){return false;}

			uint32 A;
			uint32 B;
			PCGEx::H64(Endpoints[i], A, B);

			const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
			const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

			if ((!StartPointIndexPtr || !EndPointIndexPtr))
			{
			FPlatformAtomics::InterlockedExchange(&bValid, 1);
			return false;
			}

			OutEdges[i] = FEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIOIndex);
		)
	}

	return static_cast<bool>(bValid);
}

namespace PCGExGraph
{
#define PCGEX_FOREACH_POINTEDGE_METADATA(MACRO)\
MACRO(IsIntersector, bWriteIsIntersector, IsIntersector,TEXT("bIsIntersector"))

#define PCGEX_FOREACH_EDGEEDGE_METADATA(MACRO)\
MACRO(Crossing, bWriteCrossing, Crossing,TEXT("bCrossing"))

#define PCGEX_GRAPH_META_FWD(_NAME, _ACCESSOR, _ACCESSOR2, _DEFAULT)	bWrite##_NAME = InDetails._ACCESSOR; _NAME##AttributeName = InDetails._ACCESSOR2##AttributeName; PCGEX_SOFT_VALIDATE_NAME(bWrite##_NAME, _NAME##AttributeName, Context)

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExPointPointIntersectionDetails& InDetails)
	{
		Grab(Context, InDetails.PointUnionData);
		Grab(Context, InDetails.EdgeUnionData);
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExPointEdgeIntersectionDetails& InDetails)
	{
		PCGEX_FOREACH_POINTEDGE_METADATA(PCGEX_GRAPH_META_FWD);
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExEdgeEdgeIntersectionDetails& InDetails)
	{
		PCGEX_FOREACH_EDGEEDGE_METADATA(PCGEX_GRAPH_META_FWD);
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExPointUnionMetadataDetails& InDetails)
	{
		bWriteIsPointUnion = InDetails.bWriteIsUnion;
		IsPointUnionAttributeName = InDetails.IsUnionAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsPointUnion, IsPointUnionAttributeName, Context)

		bWritePointUnionSize = InDetails.bWriteUnionSize;
		PointUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWritePointUnionSize, PointUnionSizeAttributeName, Context)
	}

	void FGraphMetadataDetails::Grab(const FPCGContext* Context, const FPCGExEdgeUnionMetadataDetails& InDetails)
	{
		bWriteIsEdgeUnion = InDetails.bWriteIsUnion;
		IsEdgeUnionAttributeName = InDetails.IsUnionAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsEdgeUnion, IsEdgeUnionAttributeName, Context)

		bWriteIsSubEdge = InDetails.bWriteIsSubEdge;
		IsSubEdgeAttributeName = InDetails.IsSubEdgeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteIsSubEdge, IsSubEdgeAttributeName, Context)

		bWriteEdgeUnionSize = InDetails.bWriteUnionSize;
		EdgeUnionSizeAttributeName = InDetails.UnionSizeAttributeName;
		PCGEX_SOFT_VALIDATE_NAME(bWriteEdgeUnionSize, EdgeUnionSizeAttributeName, Context);
	}

#undef PCGEX_FOREACH_POINTEDGE_METADATA
#undef PCGEX_FOREACH_EDGEEDGE_METADATA
#undef PCGEX_GRAPH_META_FWD

	FGraphNodeMetadata::FGraphNodeMetadata(const int32 InNodeIndex, const EPCGExIntersectionType InType)
		: NodeIndex(InNodeIndex), Type(InType)
	{
	}

	FGraphEdgeMetadata::FGraphEdgeMetadata(const int32 InEdgeIndex, const int32 InRootIndex, const EPCGExIntersectionType InType)
		: EdgeIndex(InEdgeIndex), RootIndex(InRootIndex < 0 ? InEdgeIndex : InRootIndex), Type(InType)
	{
	}

	FNode::FNode(const int32 InNodeIndex, const int32 InPointIndex)
		: Index(InNodeIndex), PointIndex(InPointIndex)
	{
		Links.Empty();
	}

	bool FNode::IsAdjacentTo(const int32 OtherNodeIndex) const
	{
		for (const FLink Lk : Links) { if (Lk.Node == OtherNodeIndex) { return true; } }
		return false;
	}

	int32 FNode::GetEdgeIndex(const int32 AdjacentNodeIndex) const
	{
		for (const FLink Lk : Links) { if (Lk.Node == AdjacentNodeIndex) { return Lk.Edge; } }
		return -1;
	}

#undef PCGEX_FOREACH_EDGE_METADATA

	FGraph::FGraph(const int32 InNumNodes)
	{
		int32 StartNodeIndex = 0;
		AddNodes(InNumNodes, StartNodeIndex);
	}

	void FGraph::ReserveForEdges(const int32 UpcomingAdditionCount, bool bReserveMeta)
	{
		UniqueEdges.Reserve(UniqueEdges.Num() + UpcomingAdditionCount);
		Edges.Reserve(Edges.Num() + UpcomingAdditionCount);

		if (bReserveMeta)
		{
			EdgeMetadata.Reserve(EdgeMetadata.Num() + UpcomingAdditionCount);
			NodeMetadata.Reserve(NodeMetadata.Num() + UpcomingAdditionCount);
		}
	}

	bool FGraph::InsertEdge_Unsafe(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex)
	{
		check(A != B)

		const uint64 Hash = PCGEx::H64U(A, B);
		if (const int32* EdgeIndex = UniqueEdges.Find(Hash))
		{
			OutEdge.Index = *EdgeIndex;
			return false;
		}

		OutEdge = Edges.Emplace_GetRef(Edges.Num(), A, B, -1, IOIndex);
		UniqueEdges.Add(Hash, (OutEdge.Index = Edges.Num() - 1));

		Nodes[A].LinkEdge(OutEdge.Index);
		Nodes[B].LinkEdge(OutEdge.Index);

		return true;
	}

	bool FGraph::InsertEdge(const int32 A, const int32 B, FEdge& OutEdge, const int32 IOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		return InsertEdge_Unsafe(A, B, OutEdge, IOIndex);
	}

	bool FGraph::InsertEdge_Unsafe(const FEdge& Edge)
	{
		uint64 H = Edge.H64U();
		if (UniqueEdges.Contains(H)) { return false; }

		FEdge& NewEdge = Edges.Emplace_GetRef(Edge);
		UniqueEdges.Add(H, (NewEdge.Index = Edges.Num() - 1));

		Nodes[Edge.Start].LinkEdge(NewEdge.Index);
		Nodes[Edge.End].LinkEdge(NewEdge.Index);

		return true;
	}

	bool FGraph::InsertEdge(const FEdge& Edge)
	{
		FWriteScopeLock WriteLock(GraphLock);
		return InsertEdge_Unsafe(Edge);
	}

	bool FGraph::InsertEdge_Unsafe(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex)
	{
		return InsertEdge_Unsafe(Edge.Start, Edge.End, OutEdge, InIOIndex);
	}

	bool FGraph::InsertEdge(const FEdge& Edge, FEdge& OutEdge, const int32 InIOIndex)
	{
		return InsertEdge(Edge.Start, Edge.End, OutEdge, InIOIndex);
	}

	void FGraph::InsertEdges(const TArray<uint64>& InEdges, const int32 InIOIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::InsertEdges)

		FWriteScopeLock WriteLock(GraphLock);
		uint32 A;
		uint32 B;

		UniqueEdges.Reserve(UniqueEdges.Num() + UniqueEdges.Num());
		Edges.Reserve(Edges.Num() + InEdges.Num());

		for (const uint64 E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			PCGEx::H64(E, A, B);

			check(A != B)

			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B, -1, InIOIndex);

			UniqueEdges.Add(E, EdgeIndex);
			Nodes[A].LinkEdge(EdgeIndex);
			Nodes[B].LinkEdge(EdgeIndex);
		}

		UniqueEdges.Shrink();
	}

	int32 FGraph::InsertEdges(const TArray<FEdge>& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::InsertEdges)

		FWriteScopeLock WriteLock(GraphLock);
		const int32 StartIndex = Edges.Num();

		UniqueEdges.Reserve(UniqueEdges.Num() + InEdges.Num());
		Edges.Reserve(Edges.Num() + InEdges.Num());

		for (const FEdge& E : InEdges) { InsertEdge_Unsafe(E); }
		return StartIndex;
	}

	FEdge* FGraph::FindEdge_Unsafe(const uint64 Hash)
	{
		const int32* Index = UniqueEdges.Find(Hash);
		if (!Index) { return nullptr; }
		return (Edges.GetData() + *Index);
	}

	FEdge* FGraph::FindEdge_Unsafe(const int32 A, const int32 B)
	{
		return FindEdge(PCGEx::H64U(A, B));
	}

	FEdge* FGraph::FindEdge(const uint64 Hash)
	{
		FReadScopeLock ReadScopeLock(GraphLock);
		const int32* Index = UniqueEdges.Find(Hash);
		if (!Index) { return nullptr; }
		return (Edges.GetData() + *Index);
	}

	FEdge* FGraph::FindEdge(const int32 A, const int32 B)
	{
		return FindEdge(PCGEx::H64U(A, B));
	}

	FGraphEdgeMetadata& FGraph::GetOrCreateEdgeMetadata(const int32 EdgeIndex, const int32 RootIndex)
	{
		{
			FReadScopeLock ReadScopeLock(MetadataLock);
			if (FGraphEdgeMetadata* MetadataPtr = EdgeMetadata.Find(EdgeIndex)) { return *MetadataPtr; }
		}
		{
			FWriteScopeLock WriteScopeLock(MetadataLock);
			return EdgeMetadata.FindOrAdd(EdgeIndex, FGraphEdgeMetadata(EdgeIndex, RootIndex));
		}
	}

	void FGraph::InsertEdges_Unsafe(const TSet<uint64>& InEdges, const int32 InIOIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::InsertEdges_Unsafe);

		uint32 A;
		uint32 B;

		UniqueEdges.Reserve(UniqueEdges.Num() + InEdges.Num());
		Edges.Reserve(UniqueEdges.Num() + InEdges.Num());

		for (const uint64& E : InEdges)
		{
			if (UniqueEdges.Contains(E)) { continue; }

			PCGEx::H64(E, A, B);

			check(A != B)

			const int32 EdgeIndex = Edges.Emplace(Edges.Num(), A, B);
			UniqueEdges.Add(E, EdgeIndex);
			Nodes[A].LinkEdge(EdgeIndex);
			Nodes[B].LinkEdge(EdgeIndex);
			Edges[EdgeIndex].IOIndex = InIOIndex;
		}
	}

	void FGraph::InsertEdges(const TSet<uint64>& InEdges, const int32 InIOIndex)
	{
		FWriteScopeLock WriteLock(GraphLock);
		InsertEdges_Unsafe(InEdges, InIOIndex);
	}

	TArrayView<FNode> FGraph::AddNodes(const int32 NumNewNodes, int32& OutStartIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::AddNodes);

		FWriteScopeLock WriteLock(GraphLock);
		OutStartIndex = Nodes.Num();
		const int32 TotalNum = OutStartIndex + NumNewNodes;
		Nodes.Reserve(TotalNum);
		for (int i = OutStartIndex; i < TotalNum; i++) { Nodes.Emplace(i, i); }

		return MakeArrayView(Nodes.GetData() + OutStartIndex, NumNewNodes);
	}

	void FGraph::BuildSubGraphs(const FPCGExGraphBuilderDetails& Limits, TArray<int32>& OutValidNodes)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FGraph::BuildSubGraphs);

		const int32 NumNodes = Nodes.Num();
		const int32 NumEdges = Edges.Num();

		TArray<bool> VisitedNodes;
		VisitedNodes.Init(false, NumNodes);
		TArray<bool> VisitedEdges;
		VisitedEdges.Init(false, NumEdges);

		int32 VisitedNodesNum = 0;
		int32 VisitedEdgesNum = 0;

		TArray<int32> Stack;
		Stack.Reserve(NumNodes);
		OutValidNodes.Reserve(NumNodes);

		for (int32 i = 0; i < NumNodes; i++)
		{
			FNode& CurrentNode = Nodes[i];
			if (VisitedNodes[i])
			{
				continue;
			}

			if (!CurrentNode.bValid || CurrentNode.IsEmpty())
			{
				CurrentNode.bValid = false;
				continue;
			}

			Stack.Reset();
			Stack.Add(i);
			VisitedNodes[i] = true;
			VisitedNodesNum++;

			TSharedPtr<FSubGraph> SubGraph = MakeShared<FSubGraph>();
			SubGraph->WeakParentGraph = SharedThis(this);
			SubGraph->Nodes.Reserve(NumNodes - VisitedNodesNum);
			SubGraph->Edges.Reserve(NumEdges - VisitedEdgesNum);

			while (!Stack.IsEmpty())
			{
				const int32 NodeIndex = Stack.Pop(EAllowShrinking::No);
				SubGraph->Nodes.Add(NodeIndex);
				FNode& Node = Nodes[NodeIndex];
				Node.NumExportedEdges = 0;

				for (const FLink& Lk : Node.Links)
				{
					const int32 E = Lk.Edge;
					if (VisitedEdges[E]) { continue; }

					VisitedEdges[E] = true;
					VisitedEdgesNum++;

					FEdge& Edge = Edges[E];
					if (!Edge.bValid) { continue; }

					const int32 OtherIndex = Edge.Other(NodeIndex);
					if (!Nodes[OtherIndex].bValid) { continue; }

					Node.NumExportedEdges++;
					SubGraph->Add(Edge);

					if (!VisitedNodes[OtherIndex])
					{
						VisitedNodes[OtherIndex] = true;
						VisitedNodesNum++;
						Stack.Add(OtherIndex);
					}
				}
			}

			if (!Limits.IsValid(SubGraph->Nodes.Num(), SubGraph->Edges.Num()))
			{
				for (const int32 j : SubGraph->Nodes) { Nodes[j].bValid = false; }
				for (const PCGEx::FIndexKey j : SubGraph->Edges) { Edges[j.Index].bValid = false; }
			}
			else if (!SubGraph->Edges.IsEmpty())
			{
				OutValidNodes.Append(SubGraph->Nodes);
				SubGraph->Shrink();
				SubGraphs.Add(SubGraph.ToSharedRef());
			}
		}
	}

	void FGraph::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const FLink Lk : RootNode.Links)
		{
			const FEdge& Edge = Edges[Lk.Edge];
			if (!Edge.bValid) { continue; }

			int32 OtherIndex = Edge.Other(FromIndex);
			if (OutIndices.Contains(OtherIndex)) { continue; }

			OutIndices.Add(OtherIndex);
			if (NextDepth > 0) { GetConnectedNodes(OtherIndex, OutIndices, NextDepth); }
		}
	}

	bool BuildEndpointsLookup(const TSharedPtr<PCGExData::FPointIO>& InPointIO, TMap<uint32, int32>& OutIndices, TArray<int32>& OutAdjacency)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExGraph::BuildLookupTable);

		PCGExArrayHelpers::InitArray(OutAdjacency, InPointIO->GetNum());
		OutIndices.Empty();

		const TUniquePtr<PCGExData::TArrayBuffer<int64>> IndexBuffer = MakeUnique<PCGExData::TArrayBuffer<int64>>(InPointIO.ToSharedRef(), Attr_PCGExVtxIdx);
		if (!IndexBuffer->InitForRead()) { return false; }

		const TArray<int64>& Indices = *IndexBuffer->GetInValues().Get();

		OutIndices.Reserve(Indices.Num());
		for (int i = 0; i < Indices.Num(); i++)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Indices[i], A, B);

			OutIndices.Add(A, i);
			OutAdjacency[i] = B;
		}

		return true;
	}

	bool IsPointDataVtxReady(const UPCGMetadata* Metadata)
	{
		return PCGExMetaHelpers::TryGetConstAttribute<int64>(Metadata, Attr_PCGExVtxIdx) ? true : false;
	}

	bool IsPointDataEdgeReady(const UPCGMetadata* Metadata)
	{
		return PCGExMetaHelpers::TryGetConstAttribute<int64>(Metadata, Attr_PCGExEdgeIdx) ? true : false;
	}

	void CleanupVtxData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		if (!PointIO->GetOut()) { return; }
		UPCGMetadata* Metadata = PointIO->GetOut()->MutableMetadata();
		PointIO->Tags->Remove(TagStr_PCGExCluster);
		PointIO->Tags->Remove(TagStr_PCGExVtx);
		Metadata->DeleteAttribute(Attr_PCGExVtxIdx);
		Metadata->DeleteAttribute(Attr_PCGExEdgeIdx);
	}

	void CleanupEdgeData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		if (!PointIO->GetOut()) { return; }
		UPCGMetadata* Metadata = PointIO->GetOut()->MutableMetadata();
		PointIO->Tags->Remove(TagStr_PCGExCluster);
		PointIO->Tags->Remove(TagStr_PCGExEdges);
		Metadata->DeleteAttribute(Attr_PCGExVtxIdx);
		Metadata->DeleteAttribute(Attr_PCGExEdgeIdx);
	}

	void CleanupClusterData(const TSharedPtr<PCGExData::FPointIO>& PointIO)
	{
		CleanupVtxData(PointIO);
		CleanupEdgeData(PointIO);
		CleanupClusterTags(PointIO);
	}
}
