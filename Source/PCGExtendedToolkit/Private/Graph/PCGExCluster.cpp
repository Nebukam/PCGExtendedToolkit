// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"

namespace PCGExCluster
{
	FNode::~FNode()
	{
		AdjacentNodes.Empty();
		Edges.Empty();
	}

	void FNode::AddConnection(const int32 InEdgeIndex, const int32 InNodeIndex)
	{
		Edges.AddUnique(InEdgeIndex);
		AdjacentNodes.AddUnique(InNodeIndex);
	}

	bool FNode::GetNormal(FCluster* InCluster, FVector& OutNormal)
	{
		if (AdjacentNodes.IsEmpty()) { return false; }

		for (int32 I : AdjacentNodes)
		{
			FVector E1 = (InCluster->Nodes[I].Position - Position).GetSafeNormal();
			FVector Perp = FVector::CrossProduct(FVector::UpVector, E1).GetSafeNormal();
			OutNormal += FVector::CrossProduct(E1, Perp).GetSafeNormal();
		}

		OutNormal /= static_cast<double>(AdjacentNodes.Num());

		return true;
	}

	FCluster::FCluster()
	{
		PointIndexMap.Empty();
		Nodes.Empty();
		Edges.Empty();
		Bounds = FBox(ForceInit);
	}

	FCluster::~FCluster()
	{
		Nodes.Empty();
		PointIndexMap.Empty();
		Edges.Empty();
	}

	FNode& FCluster::GetOrCreateNode(const int32 PointIndex, const TArray<FPCGPoint>& InPoints)
	{
		if (const int32* NodeIndex = PointIndexMap.Find(PointIndex)) { return Nodes[*NodeIndex]; }

		FNode& Node = Nodes.Emplace_GetRef();
		const int32 NodeIndex = Nodes.Num() - 1;

		PointIndexMap.Add(PointIndex, NodeIndex);

		Node.PointIndex = PointIndex;
		Node.NodeIndex = NodeIndex;
		Node.Position = InPoints[PointIndex].Transform.GetLocation();

		Bounds += Node.Position;

		return Node;
	}

	bool FCluster::BuildFrom(const PCGExData::FPointIO& InPoints, const PCGExData::FPointIO& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		PCGEx::TFAttributeReader<int32>* IndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeIndex);
		if (!IndexReader->Bind(const_cast<PCGExData::FPointIO&>(InPoints)))
		{
			PCGEX_DELETE(IndexReader)
			return false;
		}

		TMap<int32, int32> CachedPointIndices;
		CachedPointIndices.Reserve(IndexReader->Values.Num());
		for (int i = 0; i < IndexReader->Values.Num(); i++) { CachedPointIndices.Add(IndexReader->Values[i], i); }
		PCGEX_DELETE(IndexReader)

		PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
		if (!StartIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges)))
		{
			PCGEX_DELETE(StartIndexReader)
			return false;
		}

		PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);
		if (!EndIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges)))
		{
			PCGEX_DELETE(EndIndexReader)
			return false;
		}

		bool bHasInvalidEdges = false;

		const TArray<FPCGPoint>& InNodePoints = InPoints.GetIn()->GetPoints();
		const int32 NumNodes = InNodePoints.Num();
		Nodes.Reset(NumNodes);
		PointIndexMap.Empty(NumNodes);

		const TArray<FPCGPoint>& InEdgesPoints = InEdges.GetIn()->GetPoints();
		const int32 NumEdges = InEdgesPoints.Num();
		Edges.Reset(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const int32* NodeStartPtr = CachedPointIndices.Find(StartIndexReader->Values[i]);
			const int32* NodeEndPtr = CachedPointIndices.Find(EndIndexReader->Values[i]);

			if (!NodeStartPtr || !NodeEndPtr)
			{
				bHasInvalidEdges = true;
				break;
			}

			const int32 NodeStart = *NodeStartPtr;
			const int32 NodeEnd = *NodeEndPtr;

			if (!InNodePoints.IsValidIndex(NodeStart) ||
				!InNodePoints.IsValidIndex(NodeEnd) ||
				NodeStart == NodeEnd)
			{
				bHasInvalidEdges = true;
				break;
			}

			Edges.Emplace_GetRef(i, NodeStart, NodeEnd);

			FNode& Start = GetOrCreateNode(NodeStart, InNodePoints);
			FNode& End = GetOrCreateNode(NodeEnd, InNodePoints);

			Start.AddConnection(i, End.NodeIndex);
			End.AddConnection(i, Start.NodeIndex);
		}

		CachedPointIndices.Empty();
		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)

		if (!bHasInvalidEdges)
		{
			PCGEx::TFAttributeReader<int32>* EdgeNumReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgesNum);
			if (EdgeNumReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges)))
			{
				for (const FNode& Node : Nodes)
				{
					if (EdgeNumReader->Values[Node.PointIndex] != Node.AdjacentNodes.Num())
					{
						bHasInvalidEdges = true;
						break;
					}
				}
			}
			PCGEX_DELETE(EdgeNumReader)
		}

		return !bHasInvalidEdges;
	}

	int32 FCluster::FindClosestNode(const FVector& Position) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;
		for (const FNode& Vtx : Nodes)
		{
			const double Dist = FVector::DistSquared(Position, Vtx.Position);
			if (Dist < MaxDistance)
			{
				MaxDistance = Dist;
				ClosestIndex = Vtx.NodeIndex;
			}
		}

		return ClosestIndex;
	}

	const FNode& FCluster::GetNodeFromPointIndex(const int32 Index) const { return Nodes[*PointIndexMap.Find(Index)]; }
}
