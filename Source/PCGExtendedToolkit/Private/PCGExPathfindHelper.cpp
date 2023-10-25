// PCGPathfindHelper.cpp

#include "PCGExPathfindHelper.h"
#include "Containers/Array.h"
#include "Algo/Reverse.h"
#include "PCGExPriorityQueue.h"
#include "Containers/Array.h"
#include <limits>

float UPCGExPathfindHelper::Heuristic(const FGraphNode& Node, const FGraphNode& Goal)
{
    return Node.Position.GetEuclideanDistance(Goal.Position);
}

float UPCGExPathfindHelper::Cost(const FGraphNode& Node1, const FGraphNode& Node2)
{
    float DistanceXY = FVector2D(Node1.Position.X - Node2.Position.X, Node1.Position.Y - Node2.Position.Y).Size();
    float DistanceZ = FMath::Abs(Node1.Position.Z - Node2.Position.Z);

    // Define a threshold for the maximum allowed Z difference for walking on slopes
    float MaxAllowedZDifference = 100.0f;

    // Define a factor to multiply the Z difference for cost calculation
    float ZFactor = 1; // Change this factor as per your requirement.

    // Check if the Z difference exceeds the maximum allowed threshold
    if (DistanceZ > MaxAllowedZDifference)
    {
        // Assign a very high cost to discourage walking on steep slopes
        return 999999.0f;
    }

    return DistanceXY + ZFactor * DistanceZ;
}

TArray<FGraphNode> UPCGExPathfindHelper::GetNeighbors(FGraphNode CurrentNode, TArray<FGraphNode> PathPoints)
{
    TArray<FGraphNode> Neighbors;

    // Assuming ConnectedNodeIndices is an array of indices
    for (int Index : CurrentNode.ConnectedNodeIndices)
    {
        // Check if the index is valid
        if (Index >= 0 && Index < PathPoints.Num())
        {
            Neighbors.Add(PathPoints[Index]);
        }
    }

    return Neighbors;
}



void UPCGExPathfindHelper::SetupNodeConnections(TArray<FGraphNode>& Nodes, FGraphNode& StartNode, FGraphNode& EndNode) 
{
    if (Nodes.Num() == 0)
    {
        return;
    }

    FVector MinPosition = Nodes[0].Position;
    FVector MaxPosition = Nodes[0].Position;

    // Find the minimum and maximum positions
    for (const FGraphNode& gnode : Nodes)
    {
        MinPosition = MinPosition.ComponentMin(gnode.Position);
        MaxPosition = MaxPosition.ComponentMax(gnode.Position);
    }

    // Calculate the dimensions
    FVector Dimensions = MaxPosition - MinPosition;

    // Assuming uniform grid, calculate the number of nodes in each dimension
    int32 NodeCountX = 1, NodeCountY = 1, NodeCountZ = 1;
    for (const FGraphNode& Node : Nodes)
    {
        if (FMath::IsNearlyEqual(Node.Position.X, MinPosition.X))
            NodeCountY *= (FMath::IsNearlyEqual(Node.Position.Y, MinPosition.Y) ? NodeCountZ++ : NodeCountZ);
        else if (FMath::IsNearlyEqual(Node.Position.Y, MinPosition.Y))
            NodeCountX++;
    }

    // Calculate the distances between nodes in each dimension
    float DistanceX = Dimensions.X / (NodeCountX - 1);
    float DistanceY = Dimensions.Y / (NodeCountY - 1);
    float DistanceZ = NodeCountZ > 1 ? Dimensions.Z / (NodeCountZ - 1) : FLT_MAX;

    // Calculate the maximum connection distance (considering diagonally adjacent nodes)
    float ConnectionDistance = FMath::Sqrt(FMath::Pow(DistanceX, 2) + FMath::Pow(DistanceY, 2) + FMath::Pow(DistanceZ, 2));

    // Now setup the connections using the calculated connection distance
    for (int i = 0; i < Nodes.Num(); ++i)
    {
        for (int j = i + 1; j < Nodes.Num(); ++j)
        {
            // Check if the nodes are within the connection distance and not the same node
            if (Nodes[i].Position != Nodes[j].Position &&
                FVector::Dist(Nodes[i].Position, Nodes[j].Position) <= ConnectionDistance)
            {
                // If they are, add each node to the other's list of connections
                Nodes[i].ConnectedNodeIndices.Add(j);
                Nodes[j].ConnectedNodeIndices.Add(i);
            }
        }
    }

    // Add connections for StartNode and EndNode
    for (int i = 0; i < Nodes.Num(); ++i)
    {
        if (FVector::Dist(StartNode.Position, Nodes[i].Position) <= ConnectionDistance)
        {
            StartNode.ConnectedNodeIndices.Add(i);
            Nodes[i].ConnectedNodeIndices.Add(Nodes.IndexOfByKey(StartNode));
        }

        if (FVector::Dist(EndNode.Position, Nodes[i].Position) <= ConnectionDistance)
        {
            EndNode.ConnectedNodeIndices.Add(i);
            Nodes[i].ConnectedNodeIndices.Add(Nodes.IndexOfByKey(EndNode));
        }
    }


    // Output some debug info to ensure connections are set up correctly
    for (int i = 0; i < Nodes.Num(); ++i)
    {
        FString Neighbors;
        for (int Index : Nodes[i].ConnectedNodeIndices)
        {
            if (Index >= 0 && Index < Nodes.Num())
            {
                Neighbors += FString::Printf(TEXT("(%f, %f, %f) "), Nodes[Index].Position.X, Nodes[Index].Position.Y, Nodes[Index].Position.Z);
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("Node at position (%f, %f, %f) is connected to: %s"), Nodes[i].Position.X, Nodes[i].Position.Y, Nodes[i].Position.Z, *Neighbors);
    }
}



TArray<FPCGPoint> UPCGExPathfindHelper::FindPath(FPCGPoint StartPoint, FPCGPoint EndPoint, TArray<FPCGPoint> PathPoints)
{

    // Create nodes for the start and end points
    FGraphNode StartNode = { StartPoint.Transform.GetLocation(), {} };
    FGraphNode EndNode = { EndPoint.Transform.GetLocation(), {} };

    // Create a graph from path points
    TArray<FGraphNode> GraphNodes;
    for (FPCGPoint Point : PathPoints)
    {
        FGraphNode Node = { Point.Transform.GetLocation(), {} };
        if (Point.Transform.Equals(StartPoint.Transform)) {
            StartNode = Node;
        }
        else if (Point.Transform.Equals(EndPoint.Transform)) {
            EndNode = Node;
        }
        GraphNodes.Add(Node);
    }

    SetupNodeConnections(GraphNodes, StartNode, EndNode);

    // Constants for handling negative costs
    const float CostOffset = 100000.0f;

    PCGExPriorityQueue<FGraphNode> Frontier;

    // Use the index in GraphNodes array as the key in CameFrom and CostSoFar maps
    TMap<int32, FGraphNode> CameFrom;
    TArray<float> CostSoFar;
    CostSoFar.Init(MAX_FLT, GraphNodes.Num());

    // Initialize start node cost to 0
    int32 StartNodeIndex = GraphNodes.IndexOfByKey(StartNode);
    CostSoFar[StartNodeIndex] = 0;

    Frontier.Enqueue(StartNode, 0);
    CameFrom.Add(StartNodeIndex, StartNode);

    while (!Frontier.IsEmpty())
    {
        FGraphNode Current = Frontier.Dequeue();

        if (Current.Position == EndNode.Position)
        {
            break;
        }

        int32 CurrentIndex = GraphNodes.IndexOfByKey(Current);

        for (FGraphNode Next : GetNeighbors(Current, GraphNodes))
        {
            int32 NextIndex = GraphNodes.IndexOfByKey(Next);

            float NewCost = CostSoFar[CurrentIndex] + Cost(Current, Next);
            float HeuristicValue = Heuristic(Next, EndNode);

            // Add CostOffset to all costs to handle negative costs
            //NewCost += CostOffset;

            if (NewCost < CostSoFar[NextIndex] && NewCost != 999999.0f)
            {
                CostSoFar[NextIndex] = NewCost;
                float Priority = NewCost + HeuristicValue;
                Frontier.Enqueue(Next, Priority);
                CameFrom.Add(NextIndex, Current);
            }
        }
    }

    TArray<FPCGPoint> Path;
    FGraphNode Current = EndNode;

    // Use a stack to reverse the order of nodes
    TArray<FGraphNode> PathNodes;
    const float DistanceThreshold = 0.1f; // Adjust this threshold as needed
    while (FVector::Dist(Current.Position, StartNode.Position) > DistanceThreshold) {
        int32 CurrentIndex = GraphNodes.IndexOfByKey(Current);
        if (!CameFrom.Contains(CurrentIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find the previous node for node at position (%f, %f, %f)"), Current.Position.X, Current.Position.Y, Current.Position.Z);
            return TArray<FPCGPoint>(); // return an empty path to indicate an error
        }

        PathNodes.Add(Current);
        Current = CameFrom[CurrentIndex];
    }

    // Add the start node to the path
    PathNodes.Add(StartNode);

    // Reverse the order of nodes
    Algo::Reverse(PathNodes);

    // Convert path nodes to FPCGPoint
    for (const FGraphNode& Node : PathNodes)
    {
        Path.Add(ConvertToPCGPoint(FCustomPoint(Node.Position)));
    }

    return Path;
}



FCustomPoint UPCGExPathfindHelper::ConvertToFVector(const FPCGPoint& Point)
{
    return FCustomPoint(Point.Transform.GetLocation().X, Point.Transform.GetLocation().Y, Point.Transform.GetLocation().Z);
}

FPCGPoint UPCGExPathfindHelper::ConvertToPCGPoint(const FCustomPoint& Point)
{
    FPCGPoint Result;
    Result.Transform.SetLocation(FVector(Point.X, Point.Y, Point.Z));
    return Result;
}