// PCGPathfindHelper.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "PCGexCustomPoint.h"

#include "PCGExPathfindHelper.generated.h"

UCLASS()
class UPCGExPathfindHelper : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PCGEx/Pathfinding")
        static TArray<FPCGPoint> FindPath(FPCGPoint StartPoint, FPCGPoint EndPoint, TArray<FPCGPoint> PathPoints);

private:
    // Utility function to convert FPCGPoint to FCustomPoint
    static FCustomPoint ConvertToFVector(const FPCGPoint& Point);

    // Utility function to convert FCustomPoint to FPCGPoint
    static FPCGPoint ConvertToPCGPoint(const FCustomPoint& Point);

    static TArray<FCustomPoint> FindShortestPathAStar(const TMap<FCustomPoint, TArray<FCustomPoint>>& Graph, const FCustomPoint& Start, const FCustomPoint& End);
    static TArray<FGraphNode> BuildGraph(const TArray<FPCGPoint>& PathPoints, float DistanceThreshold);

    static float Heuristic(const FGraphNode& Node, const FGraphNode& Goal);
    static float Cost(const FGraphNode& Node1, const FGraphNode& Node2);
    static TArray<FGraphNode> GetNeighbors(FGraphNode CurrentNode, TArray<FGraphNode> PathPoints);
    static void SetupNodeConnections(TArray<FGraphNode>& Nodes, FGraphNode& StartNode, FGraphNode& EndNode);
};
