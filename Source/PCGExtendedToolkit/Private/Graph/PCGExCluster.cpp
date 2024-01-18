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
		EdgeLengths.Empty();
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

	bool FCluster::BuildFrom(
		const PCGExData::FPointIO& InEdges,
		const TArray<FPCGPoint>& InNodePoints,
		const TMap<int32, int32>& InNodeIndicesMap,
		const TArray<int32>& PerNodeEdgeNums)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		bool bInvalidCluster = false;

		PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
		PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);

		StartIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges));
		EndIndexReader->Bind(const_cast<PCGExData::FPointIO&>(InEdges));

		const int32 NumNodes = InNodePoints.Num();
		Nodes.Reset(NumNodes);
		PointIndexMap.Empty(NumNodes);

		const TArray<FPCGPoint>& InEdgesPoints = InEdges.GetIn()->GetPoints();
		const int32 NumEdges = InEdgesPoints.Num();
		Edges.Reset(NumEdges);

		TArray<PCGExGraph::FIndexedEdge> EdgeList;
		EdgeList.SetNum(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const int32* StartPtr = InNodeIndicesMap.Find(StartIndexReader->Values[i]);
			const int32* EndPtr = InNodeIndicesMap.Find(EndIndexReader->Values[i]);

			if (!StartPtr || !EndPtr)
			{
				bInvalidCluster = true;
				break;
			}

			const int32 StartPoint = *StartPtr;
			const int32 EndPoint = *EndPtr;

			if (!InNodePoints.IsValidIndex(StartPoint) ||
				!InNodePoints.IsValidIndex(EndPoint) ||
				StartPoint == EndPoint)
			{
				bInvalidCluster = true;
				break;
			}

			EdgeList[i].Start = StartPoint;
			EdgeList[i].End = EndPoint;
			EdgeList[i].PointIndex = i;
		}

		PCGEX_DELETE(StartIndexReader)
		PCGEX_DELETE(EndIndexReader)

		if (!bInvalidCluster)
		{
			EdgeList.Sort(
				[](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
				{
					return A.Start == B.Start ? A.End < B.End : A.Start < B.Start;
				});

			for (int i = 0; i < NumEdges; i++)
			{
				const PCGExGraph::FIndexedEdge& SortedEdge = EdgeList[i];
				Edges.Emplace_GetRef(i, SortedEdge.Start, SortedEdge.End, SortedEdge.PointIndex);

				FNode& Start = GetOrCreateNode(SortedEdge.Start, InNodePoints);
				FNode& End = GetOrCreateNode(SortedEdge.End, InNodePoints);
				EdgeIndexMap.Add(PCGExGraph::GetUnsignedHash64(Start.NodeIndex, End.NodeIndex), i);

				Start.AddConnection(i, End.NodeIndex);
				End.AddConnection(i, Start.NodeIndex);
			}

			for (FNode& Node : Nodes)
			{
				if (PerNodeEdgeNums[Node.PointIndex] != Node.AdjacentNodes.Num())
				{
					bInvalidCluster = true;
					break;
				}
			}
		}

		EdgeList.Empty();

		bValid = !bInvalidCluster;
		return bValid;
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

	const PCGExGraph::FIndexedEdge& FCluster::GetEdgeFromNodeIndices(const int32 A, const int32 B) const { return Edges[*EdgeIndexMap.Find(PCGExGraph::GetUnsignedHash64(A, B))]; }

	void FCluster::ComputeEdgeLengths(const bool bNormalize)
	{
		if (!bEdgeLengthsDirty) { return; }

		const int32 NumEdges = Edges.Num();
		double Min = TNumericLimits<double>::Max();
		double Max = TNumericLimits<double>::Min();
		EdgeLengths.SetNum(NumEdges);

		for (int i = 0; i < NumEdges; i++)
		{
			const PCGExGraph::FIndexedEdge& Edge = Edges[i];
			const FNode& A = GetNodeFromPointIndex(Edge.Start);
			const FNode& B = GetNodeFromPointIndex(Edge.End);
			const double Dist = FVector::DistSquared(A.Position, B.Position);
			EdgeLengths[i] = Dist;
			Min = FMath::Min(Dist, Min);
			Max = FMath::Max(Dist, Max);
		}

		if (bNormalize) { for (int i = 0; i < NumEdges; i++) { EdgeLengths[i] = PCGExMath::Remap(EdgeLengths[i], Min, Max, 0, 1); } }

		bEdgeLengthsDirty = false;
	}
}

bool FPCGExBuildCluster::ExecuteTask()
{
	Cluster->BuildFrom(
		*EdgeIO,
		PointIO->GetIn()->GetPoints(),
		*NodeIndicesMap,
		*PerNodeEdgeNums);

	return true;
}
