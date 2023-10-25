// Fill out your copyright notice in the Description page of Project Settings.


#include "PCGExCollectionSorting.h"
#include "Containers/Array.h"
#include "Algo/Reverse.h"
#include "Containers/Array.h"
#include <limits>

void UPCGExCollectionSorting::SortByDensity(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction)
{
    /*
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

    return Path;
    */
}

void UPCGExCollectionSorting::SortByPosition(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction, ESortAxisOrder AxisOrder)
{
    
}

void UPCGExCollectionSorting::SortByScale(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction, ESortAxisOrder AxisOrder)
{

}

void UPCGExCollectionSorting::SortByRotation(UPARAM(ref) TArray<FPCGPoint>& PathPoints, ESortDirection Direction, ESortAxisOrder AxisOrder)
{

}