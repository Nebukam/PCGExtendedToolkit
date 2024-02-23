// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExCluster.h"

#include "Data/PCGExAttributeHelpers.h"
#include "Geometry/PCGExGeo.h"

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

	bool FNode::GetNormal(FCluster* InCluster, FVector& OutNormal) const
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

	int32 FNode::GetEdgeIndex(const int32 AdjacentNodeIndex) const
	{
		for (int i = 0; i < AdjacentNodes.Num(); i++) { if (AdjacentNodes[i] == AdjacentNodeIndex) { return Edges[i]; } }
		return -1;
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
		const PCGExData::FPointIO& EdgeIO,
		const TArray<FPCGPoint>& InNodePoints,
		const TMap<int32, int32>& InNodeIndicesMap,
		const TArray<int32>& PerNodeEdgeNums)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		Nodes.Empty();
		Edges.Empty();
		PointIndexMap.Empty();
		EdgeIndexMap.Empty();

		TArray<PCGExGraph::FIndexedEdge> EdgeList;
		if (!BuildIndexedEdges(EdgeIO, InNodeIndicesMap, EdgeList, true))
		{
			EdgeList.Empty();
			return false;
		}

		bool bInvalidCluster = false;

		const int32 NumNodes = InNodePoints.Num();
		Nodes.Reserve(NumNodes);
		PointIndexMap.Reserve(NumNodes);

		const int32 NumEdges = EdgeList.Num();
		Edges.SetNumUninitialized(NumEdges);
		EdgeIndexMap.Reserve(NumEdges);

		//We need to sort edges in order to have deterministic processing of the clusters
		EdgeList.Sort(
			[](const PCGExGraph::FIndexedEdge& A, const PCGExGraph::FIndexedEdge& B)
			{
				return A.Start == B.Start ? A.End < B.End : A.Start < B.Start;
			});

		for (int i = 0; i < NumEdges; i++)
		{
			PCGExGraph::FIndexedEdge& SortedEdge = (Edges[i] = EdgeList[i]);
			SortedEdge.EdgeIndex = i;

			FNode& Start = GetOrCreateNode(SortedEdge.Start, InNodePoints);
			FNode& End = GetOrCreateNode(SortedEdge.End, InNodePoints);
			EdgeIndexMap.Add(PCGEx::H64U(Start.NodeIndex, End.NodeIndex), i);

			Start.AddConnection(i, End.NodeIndex);
			End.AddConnection(i, Start.NodeIndex);
		}

		EdgeList.Empty();

		for (FNode& Node : Nodes)
		{
			if (PerNodeEdgeNums[Node.PointIndex] != Node.AdjacentNodes.Num())
			{
				bInvalidCluster = true;
				break;
			}
		}

		bValid = !bInvalidCluster;
		return bValid;
	}

	void FCluster::BuildPartialFrom(const TArray<FVector>& Positions, const TSet<uint64>& InEdges)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCluster::BuildCluster);

		const int32 NumNodes = Positions.Num();
		Nodes.Reserve(NumNodes);

		const int32 NumEdges = InEdges.Num();
		Edges.SetNumUninitialized(NumEdges);

		for (int i = 0; i < Positions.Num(); i++)
		{
			FNode& Node = Nodes.Emplace_GetRef();

			Node.PointIndex = i;
			Node.NodeIndex = i;
			Node.Position = Positions[i];
		}

		int32 EdgeIndex = 0;
		for (const uint64 Edge : InEdges)
		{
			uint32 A;
			uint32 B;
			PCGEx::H64(Edge, A, B);
			FNode& Start = Nodes[A];
			FNode& End = Nodes[B];

			Start.AddConnection(-1, End.NodeIndex);
			End.AddConnection(-1, Start.NodeIndex);
			EdgeIndex++;
		}
	}

	int32 FCluster::FindClosestNode(const FVector& Position, const int32 MinNeighbors) const
	{
		double MaxDistance = TNumericLimits<double>::Max();
		int32 ClosestIndex = -1;
		for (const FNode& Node : Nodes)
		{
			if (Node.AdjacentNodes.Num() < MinNeighbors) { continue; }
			const double Dist = FVector::DistSquared(Position, Node.Position);
			if (Dist < MaxDistance)
			{
				MaxDistance = Dist;
				ClosestIndex = Node.NodeIndex;
			}
		}

		return ClosestIndex;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighborCount) { continue; }
			if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, Nodes[OtherIndex].Position);
				Dist < LastDist)
			{
				LastDist = Dist;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighbor(const int32 NodeIndex, const FVector& Position, const TSet<int32>& Exclusion, int32 MinNeighborCount) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDist = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighborCount) { continue; }
			if (Exclusion.Contains(OtherIndex)) { continue; }
			if (const double Dist = FMath::PointDistToSegmentSquared(Position, Node.Position, Nodes[OtherIndex].Position);
				Dist < LastDist)
			{
				LastDist = Dist;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	const FNode& FCluster::GetNodeFromPointIndex(const int32 Index) const { return Nodes[*PointIndexMap.Find(Index)]; }

	const PCGExGraph::FIndexedEdge& FCluster::GetEdgeFromNodeIndices(const int32 A, const int32 B) const { return Edges[*EdgeIndexMap.Find(PCGEx::H64U(A, B))]; }

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

	void FCluster::GetConnectedNodes(const int32 FromIndex, TArray<int32>& OutIndices, const int32 SearchDepth) const
	{
		const int32 NextDepth = SearchDepth - 1;
		const FNode& RootNode = Nodes[FromIndex];

		for (const int32 OtherNode : RootNode.AdjacentNodes)
		{
			if (OutIndices.Contains(OtherNode)) { continue; }

			OutIndices.Add(OtherNode);
			if (NextDepth > 0) { GetConnectedNodes(OtherNode, OutIndices, NextDepth); }
		}
	}

	FVector FCluster::GetEdgeDirection(const int32 FromIndex, const int32 ToIndex) const
	{
		return (Nodes[FromIndex].Position - Nodes[ToIndex].Position).GetSafeNormal();
	}

	FVector FCluster::GetCentroid(const int32 NodeIndex) const
	{
		const FNode& Node = Nodes[NodeIndex];
		FVector Centroid = FVector::ZeroVector;
		for (const int32 OtherIndex : Node.AdjacentNodes) { Centroid += Nodes[OtherIndex].Position; }
		return Centroid / static_cast<double>(Node.AdjacentNodes.Num());
	}

	FVector FCluster::GetCentroid(const int32 NodeIndex, const TSet<int32>& Exclusion) const
	{
		const FNode& Node = Nodes[NodeIndex];
		double Divider = 0;

		FVector Centroid = FVector::ZeroVector;

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Exclusion.Contains(OtherIndex)) { continue; }
			Centroid += Nodes[OtherIndex].Position;
			Divider++;
		}

		return Centroid / Divider;
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const int32 MinNeighbors) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighbors) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, (Node.Position - Nodes[OtherIndex].Position).GetSafeNormal());
				Dot > LastDot)
			{
				LastDot = Dot;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const TSet<int32>& Exclusion, const int32 MinNeighbors) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			if (Nodes[OtherIndex].AdjacentNodes.Num() < MinNeighbors) { continue; }
			if (Exclusion.Contains(OtherIndex)) { continue; }
			if (const double Dot = FVector::DotProduct(Direction, (Node.Position - Nodes[OtherIndex].Position).GetSafeNormal());
				Dot > LastDot)
			{
				LastDot = Dot;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const FVector& Guide, const int32 MinNeighbors) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;
		double LastDist = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			const FNode& OtherNode = Nodes[OtherIndex];
			if (OtherNode.AdjacentNodes.Num() < MinNeighbors) { continue; }

			const double Dot = FVector::DotProduct(Direction, (Node.Position - OtherNode.Position).GetSafeNormal());

			if (Dot > LastDot)
			{
				if (FMath::IsNearlyEqual(LastDot, Dot, 0.2))
				{
					if (const double Dist = FMath::PointDistToSegmentSquared(Guide, Node.Position, OtherNode.Position);
						Dist < LastDist)
					{
						LastDist = Dist;
						LastDot = Dot;
						Result = OtherIndex;
					}
				}
				else
				{
					LastDot = Dot;
					Result = OtherIndex;
				}
			}
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighborInDirection(const int32 NodeIndex, const FVector& Direction, const FVector& Guide, const TSet<int32>& Exclusion, const int32 MinNeighbors) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastDot = -1;
		double LastDist = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			const FNode& OtherNode = Nodes[OtherIndex];
			if (OtherNode.AdjacentNodes.Num() < MinNeighbors) { continue; }
			if (Exclusion.Contains(OtherIndex)) { continue; }

			const double Dot = FVector::DotProduct(Direction, (Node.Position - OtherNode.Position).GetSafeNormal());

			if (Dot > LastDot)
			{
				if (FMath::IsNearlyEqual(LastDot, Dot, 0.2))
				{
					if (const double Dist = FMath::PointDistToSegmentSquared(Guide, Node.Position, OtherNode.Position);
						Dist < LastDist)
					{
						LastDist = Dist;
						LastDot = Dot;
						Result = OtherIndex;
					}
				}
				else
				{
					LastDot = Dot;
					Result = OtherIndex;
				}
			}
		}

		return Result;
	}

	int32 FCluster::FindClosestNeighborLeft(const int32 NodeIndex, const FVector& Direction, const TSet<int32>& Exclusion, const int32 MinNeighbors) const
	{
		const FNode& Node = Nodes[NodeIndex];
		int32 Result = -1;
		double LastAngle = TNumericLimits<double>::Max();

		for (const int32 OtherIndex : Node.AdjacentNodes)
		{
			const FNode& OtherNode = Nodes[OtherIndex];

			if (OtherNode.AdjacentNodes.Num() < MinNeighbors) { continue; }

			if (Exclusion.Contains(OtherIndex)) { continue; }

			const double Angle = PCGExMath::GetAngle(Direction, (Node.Position - OtherNode.Position).GetSafeNormal());

			if (Angle < LastAngle)
			{
				LastAngle = Angle;
				Result = OtherIndex;
			}
		}

		return Result;
	}

	void FCluster::ProjectNodes(const FPCGExGeo2DProjectionSettings& ProjectionSettings)
	{
		for (FNode& Node : Nodes)
		{
			const FVector V = ProjectionSettings.ProjectionTransform.InverseTransformPosition(Node.Position);
			Node.Position = FVector(V.X, V.Y, 0);
		}
	}
}

namespace PCGExClusterTask
{
	bool FBuildCluster::ExecuteTask()
	{
		Cluster->BuildFrom(
			*EdgeIO,
			PointIO->GetIn()->GetPoints(),
			*NodeIndicesMap,
			*PerNodeEdgeNums);

		return true;
	}

	bool FFindNodeChains::ExecuteTask()
	{
		return true;
	}
}
