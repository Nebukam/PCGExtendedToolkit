// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExClusterHelpers.h"

namespace PCGExTest
{
#pragma region FTestCluster

	void FTestCluster::Initialize(
		const TSharedPtr<PCGEx::FIndexLookup>& InNodeIndexLookup,
		const TSharedPtr<TArray<PCGExClusters::FNode>>& InNodes,
		const TSharedPtr<TArray<PCGExGraphs::FEdge>>& InEdges,
		const TArray<FVector>& InPositions)
	{
		NodeIndexLookup = InNodeIndexLookup;
		Nodes = InNodes;
		Edges = InEdges;
		Positions = InPositions;

		NodesDataPtr = Nodes->GetData();
		EdgesDataPtr = Edges->GetData();

		NumRawVtx = InPositions.Num();
		NumRawEdges = InEdges->Num();

		bValid = true;

		// Compute bounds
		Bounds = FBox(ForceInit);
		for (const FVector& Pos : Positions)
		{
			Bounds += Pos;
		}
		Bounds = Bounds.ExpandBy(10);
	}

	void FTestCluster::SetCachedData(FName Key, const TSharedPtr<PCGExClusters::ICachedClusterData>& Data)
	{
		FWriteScopeLock WriteLock(ClusterLock);
		CachedData.Add(Key, Data);
	}

	void FTestCluster::ClearCachedData()
	{
		FWriteScopeLock WriteLock(ClusterLock);
		CachedData.Empty();
	}

#pragma endregion

#pragma region FClusterBuilder

	FClusterBuilder::FClusterBuilder()
	{
	}

	FClusterBuilder::~FClusterBuilder()
	{
	}

	FClusterBuilder& FClusterBuilder::AddNode(const int32 PointIndex, const FVector& Position)
	{
		if (!PointToNodeIndex.Contains(PointIndex))
		{
			PointToNodeIndex.Add(PointIndex, Positions.Num());
			Positions.Add(Position);
		}
		return *this;
	}

	FClusterBuilder& FClusterBuilder::AddEdge(const int32 StartPointIndex, const int32 EndPointIndex)
	{
		EdgeDefinitions.Add(TPair<int32, int32>(StartPointIndex, EndPointIndex));
		return *this;
	}

	FClusterBuilder& FClusterBuilder::WithLinearChain(const int32 NumNodes, const double Spacing, const FVector& Origin)
	{
		Positions.Reset();
		EdgeDefinitions.Reset();
		PointToNodeIndex.Reset();

		for (int32 i = 0; i < NumNodes; i++)
		{
			AddNode(i, Origin + FVector(i * Spacing, 0, 0));
		}

		for (int32 i = 0; i < NumNodes - 1; i++)
		{
			AddEdge(i, i + 1);
		}

		return *this;
	}

	FClusterBuilder& FClusterBuilder::WithClosedLoop(const int32 NumNodes, const double Radius, const FVector& Center)
	{
		Positions.Reset();
		EdgeDefinitions.Reset();
		PointToNodeIndex.Reset();

		const double AngleStep = 2.0 * PI / NumNodes;
		for (int32 i = 0; i < NumNodes; i++)
		{
			const double Angle = i * AngleStep;
			AddNode(i, Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0));
		}

		for (int32 i = 0; i < NumNodes; i++)
		{
			AddEdge(i, (i + 1) % NumNodes);
		}

		return *this;
	}

	FClusterBuilder& FClusterBuilder::WithStar(const int32 NumLeaves, const double Radius, const FVector& Center)
	{
		Positions.Reset();
		EdgeDefinitions.Reset();
		PointToNodeIndex.Reset();

		// Center node
		AddNode(0, Center);

		// Leaf nodes
		const double AngleStep = 2.0 * PI / NumLeaves;
		for (int32 i = 0; i < NumLeaves; i++)
		{
			const double Angle = i * AngleStep;
			AddNode(i + 1, Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0));
			AddEdge(0, i + 1);
		}

		return *this;
	}

	FClusterBuilder& FClusterBuilder::WithGrid(const int32 CountX, const int32 CountY, const double Spacing, const FVector& Origin)
	{
		Positions.Reset();
		EdgeDefinitions.Reset();
		PointToNodeIndex.Reset();

		// Create nodes
		for (int32 y = 0; y < CountY; y++)
		{
			for (int32 x = 0; x < CountX; x++)
			{
				const int32 Index = y * CountX + x;
				AddNode(Index, Origin + FVector(x * Spacing, y * Spacing, 0));
			}
		}

		// Create edges
		for (int32 y = 0; y < CountY; y++)
		{
			for (int32 x = 0; x < CountX; x++)
			{
				const int32 Index = y * CountX + x;

				// Horizontal edge
				if (x < CountX - 1)
				{
					AddEdge(Index, Index + 1);
				}

				// Vertical edge
				if (y < CountY - 1)
				{
					AddEdge(Index, Index + CountX);
				}
			}
		}

		return *this;
	}

	TSharedRef<FTestCluster> FClusterBuilder::Build()
	{
		TSharedRef<FTestCluster> Cluster = MakeShared<FTestCluster>();

		if (Positions.IsEmpty() || EdgeDefinitions.IsEmpty())
		{
			return Cluster;
		}

		const int32 NumNodes = Positions.Num();
		const int32 NumEdges = EdgeDefinitions.Num();

		// Create index lookup (constructor initializes all entries to -1)
		TSharedPtr<PCGEx::FIndexLookup> NodeIndexLookup = MakeShared<PCGEx::FIndexLookup>(NumNodes);

		// Allocate nodes and edges
		TSharedPtr<TArray<PCGExClusters::FNode>> Nodes = MakeShared<TArray<PCGExClusters::FNode>>();
		TSharedPtr<TArray<PCGExGraphs::FEdge>> Edges = MakeShared<TArray<PCGExGraphs::FEdge>>();

		Nodes->SetNum(NumNodes);
		Edges->SetNum(NumEdges);

		// Initialize nodes
		for (int32 i = 0; i < NumNodes; i++)
		{
			PCGExClusters::FNode& Node = (*Nodes)[i];
			Node.Index = i;
			Node.PointIndex = i; // Point index == Node index for test clusters
			Node.bValid = 1;
			NodeIndexLookup->GetMutable(i) = i;
		}

		// Initialize edges and link nodes
		for (int32 i = 0; i < NumEdges; i++)
		{
			const TPair<int32, int32>& EdgeDef = EdgeDefinitions[i];
			const int32 StartNode = PointToNodeIndex.FindRef(EdgeDef.Key);
			const int32 EndNode = PointToNodeIndex.FindRef(EdgeDef.Value);

			PCGExGraphs::FEdge& Edge = (*Edges)[i];
			Edge.Index = i;
			Edge.Start = EdgeDef.Key;
			Edge.End = EdgeDef.Value;
			Edge.PointIndex = i;
			Edge.IOIndex = 0;
			Edge.bValid = 1;

			// Link nodes
			(*Nodes)[StartNode].Link(EndNode, i);
			(*Nodes)[EndNode].Link(StartNode, i);
		}

		Cluster->Initialize(NodeIndexLookup, Nodes, Edges, Positions);
		return Cluster;
	}

#pragma endregion

#pragma region ClusterVerify

	namespace ClusterVerify
	{
		bool HasNodeCount(const TSharedRef<FTestCluster>& Cluster, const int32 ExpectedCount)
		{
			return Cluster->Nodes && Cluster->Nodes->Num() == ExpectedCount;
		}

		bool HasEdgeCount(const TSharedRef<FTestCluster>& Cluster, const int32 ExpectedCount)
		{
			return Cluster->Edges && Cluster->Edges->Num() == ExpectedCount;
		}

		bool NodeHasNeighborCount(const TSharedRef<FTestCluster>& Cluster, const int32 NodeIndex, const int32 ExpectedNeighbors)
		{
			if (!Cluster->Nodes || !Cluster->Nodes->IsValidIndex(NodeIndex))
			{
				return false;
			}
			return (*Cluster->Nodes)[NodeIndex].Num() == ExpectedNeighbors;
		}

		bool NodeIsLeaf(const TSharedRef<FTestCluster>& Cluster, const int32 NodeIndex)
		{
			if (!Cluster->Nodes || !Cluster->Nodes->IsValidIndex(NodeIndex))
			{
				return false;
			}
			return (*Cluster->Nodes)[NodeIndex].IsLeaf();
		}

		bool NodeIsBinary(const TSharedRef<FTestCluster>& Cluster, const int32 NodeIndex)
		{
			if (!Cluster->Nodes || !Cluster->Nodes->IsValidIndex(NodeIndex))
			{
				return false;
			}
			return (*Cluster->Nodes)[NodeIndex].IsBinary();
		}

		bool NodeIsComplex(const TSharedRef<FTestCluster>& Cluster, const int32 NodeIndex)
		{
			if (!Cluster->Nodes || !Cluster->Nodes->IsValidIndex(NodeIndex))
			{
				return false;
			}
			return (*Cluster->Nodes)[NodeIndex].IsComplex();
		}

		int32 CountNodesWithNeighbors(const TSharedRef<FTestCluster>& Cluster, const int32 NeighborCount)
		{
			if (!Cluster->Nodes)
			{
				return 0;
			}

			int32 Count = 0;
			for (const PCGExClusters::FNode& Node : *Cluster->Nodes)
			{
				if (Node.Num() == NeighborCount)
				{
					Count++;
				}
			}
			return Count;
		}

		int32 CountLeafNodes(const TSharedRef<FTestCluster>& Cluster)
		{
			return CountNodesWithNeighbors(Cluster, 1);
		}

		int32 CountBinaryNodes(const TSharedRef<FTestCluster>& Cluster)
		{
			return CountNodesWithNeighbors(Cluster, 2);
		}

		int32 CountComplexNodes(const TSharedRef<FTestCluster>& Cluster)
		{
			if (!Cluster->Nodes)
			{
				return 0;
			}

			int32 Count = 0;
			for (const PCGExClusters::FNode& Node : *Cluster->Nodes)
			{
				if (Node.IsComplex())
				{
					Count++;
				}
			}
			return Count;
		}
	}

#pragma endregion
}
