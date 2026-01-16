// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExCell.h"
#include "Misc/ScopeExit.h"

#include "Clusters/Artifacts/PCGExCellDetails.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Math/PCGExMath.h"
#include "Math/PCGExMathAxis.h"
#include "Math/Geo/PCGExGeo.h"
#include "Paths/PCGExPathsCommon.h"

namespace PCGExClusters
{
	void SetPointProperty(PCGExData::FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty)
	{
		if (InProperty == EPCGExPointPropertyOutput::Density)
		{
			TPCGValueRange<float> Density = InPoint.Data->GetDensityValueRange(false);
			Density[InPoint.Index] = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::Steepness)
		{
			TPCGValueRange<float> Steepness = InPoint.Data->GetSteepnessValueRange(false);
			Steepness[InPoint.Index] = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorR)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(0) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorG)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(1) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorB)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(2) = InValue;
		}
		else if (InProperty == EPCGExPointPropertyOutput::ColorA)
		{
			TPCGValueRange<FVector4> Color = InPoint.Data->GetColorValueRange(false);
			Color[InPoint.Index].Component(3) = InValue;
		}
	}

	bool FHoles::Overlaps(const TArray<FVector2D>& Polygon)
	{
		{
			FReadScopeLock ReadScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return PCGExMath::Geo::IsAnyPointInPolygon(ProjectedPoints, Polygon); }
		}

		{
			FWriteScopeLock WriteScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return PCGExMath::Geo::IsAnyPointInPolygon(ProjectedPoints, Polygon); }

			ProjectionDetails.ProjectFlat(PointDataFacade, ProjectedPoints);
			return PCGExMath::Geo::IsAnyPointInPolygon(ProjectedPoints, Polygon);
		}
	}

	FCellConstraints::FCellConstraints(const FPCGExCellConstraintsDetails& InDetails)
	{
		Winding = InDetails.OutputWinding;
		bConcaveOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConcaveOnly;
		bConvexOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConvexOnly;
		bKeepCellsWithLeaves = InDetails.bKeepCellsWithLeaves;
		bDuplicateLeafPoints = InDetails.bDuplicateLeafPoints;

		WrapperClassificationTolerance = InDetails.WrapperClassificationTolerance;
		bBuildWrapper = InDetails.bOmitWrappingBounds;

		if (InDetails.bOmitBelowPointCount) { MinPointCount = InDetails.MinPointCount; }
		if (InDetails.bOmitAbovePointCount) { MaxPointCount = InDetails.MaxPointCount; }

		if (InDetails.bOmitBelowBoundsSize) { MinBoundsSize = InDetails.MinBoundsSize; }
		if (InDetails.bOmitAboveBoundsSize) { MaxBoundsSize = InDetails.MaxBoundsSize; }

		if (InDetails.bOmitBelowArea) { MinArea = InDetails.MinArea; }
		if (InDetails.bOmitAboveArea) { MaxArea = InDetails.MaxArea; }

		if (InDetails.bOmitBelowPerimeter) { MinPerimeter = InDetails.MinPerimeter; }
		if (InDetails.bOmitAbovePerimeter) { MaxPerimeter = InDetails.MaxPerimeter; }

		if (InDetails.bOmitBelowSegmentLength) { MinSegmentLength = InDetails.MinSegmentLength; }
		if (InDetails.bOmitAboveSegmentLength) { MaxSegmentLength = InDetails.MaxSegmentLength; }

		if (InDetails.bOmitBelowCompactness) { MinCompactness = InDetails.MinCompactness; }
		if (InDetails.bOmitAboveCompactness) { MaxCompactness = InDetails.MaxCompactness; }
	}

	void FCellConstraints::Reserve(const int32 InCellHashReserve)
	{
		UniqueStartHalfEdgesHash.Reserve(InCellHashReserve);
		UniquePathsHashSet.Reserve(InCellHashReserve);
	}

	bool FCellConstraints::ContainsSignedEdgeHash(const uint64 Hash)
	{
		return UniqueStartHalfEdgesHash.Contains(Hash);
	}

	bool FCellConstraints::IsUniqueStartHalfEdge(const uint64 Hash)
	{
		bool bAlreadyExists = false;
		UniqueStartHalfEdgesHash.Add(Hash, bAlreadyExists);
		return !bAlreadyExists;
	}

	bool FCellConstraints::IsUniqueCellHash(const TSharedPtr<FCell>& InCell)
	{
		bool bAlreadyExists;
		UniquePathsHashSet.Add(InCell->GetCellHash(), bAlreadyExists);
		return !bAlreadyExists;
	}

	void FCellConstraints::BuildWrapperCell(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const TSharedPtr<FCellConstraints>& InConstraints)
	{
		// Build the DCEL and enumerate all faces
		FPlanarFaceEnumerator Enumerator;
		Enumerator.Build(InCluster, ProjectedPositions);

		// Create minimal constraints for wrapper detection - no filtering
		TSharedPtr<FCellConstraints> TempConstraints = MakeShared<FCellConstraints>();
		TempConstraints->bKeepCellsWithLeaves = true;
		TempConstraints->bDuplicateLeafPoints = InConstraints ? InConstraints->bDuplicateLeafPoints : false;

		// Enumerate all faces
		TArray<TSharedPtr<FCell>> AllCells;
		Enumerator.EnumerateAllFaces(AllCells, TempConstraints.ToSharedRef());

		// The wrapper/exterior face is simply the one with the largest area
		// In a proper planar graph, the exterior face encloses all others
		double LargestArea = -MAX_dbl;
		for (const TSharedPtr<FCell>& Cell : AllCells)
		{
			if (Cell && Cell->Data.Area > LargestArea)
			{
				LargestArea = Cell->Data.Area;
				WrapperCell = Cell;
			}
		}

		// Fallback for tree structures (no cycles = no valid DCEL faces)
		// Use DFS tree-walking to produce wrap-around path that follows all branches
		if (!WrapperCell && InCluster->Nodes->Num() >= 2)
		{
			const TArray<FNode>& Nodes = *InCluster->Nodes;
			const int32 NumNodes = Nodes.Num();

			// Find a leaf node to start from (gives cleaner traversal)
			int32 StartNode = 0;
			for (int32 i = 0; i < NumNodes; ++i)
			{
				if (Nodes[i].IsLeaf())
				{
					StartNode = i;
					break;
				}
			}

			// DFS tree walk - visits each edge twice (once each direction)
			// This produces the wrap-around path
			TArray<int32> WalkNodes;
			WalkNodes.Reserve(InCluster->Edges->Num() * 2 + 1);

			const bool bDuplicateLeaves = TempConstraints->bDuplicateLeafPoints;

			TSet<int32> VisitedNodes;
			TArray<TPair<int32, int32>> Stack; // (node, link index to process next)
			Stack.Reserve(NumNodes);
			Stack.Emplace(StartNode, 0);
			WalkNodes.Add(StartNode);
			if (bDuplicateLeaves && Nodes[StartNode].IsLeaf()) { WalkNodes.Add(StartNode); }
			VisitedNodes.Add(StartNode);

			while (!Stack.IsEmpty())
			{
				TPair<int32, int32>& Current = Stack.Last();
				const FNode& Node = Nodes[Current.Key];

				// Find next unvisited neighbor
				bool bFoundNext = false;
				while (Current.Value < Node.Links.Num())
				{
					const int32 NeighborNode = Node.Links[Current.Value].Node;
					Current.Value++;

					if (!VisitedNodes.Contains(NeighborNode))
					{
						// Go to unvisited neighbor
						VisitedNodes.Add(NeighborNode);
						WalkNodes.Add(NeighborNode);
						if (bDuplicateLeaves && Nodes[NeighborNode].IsLeaf()) { WalkNodes.Add(NeighborNode); }
						Stack.Emplace(NeighborNode, 0);
						bFoundNext = true;
						break;
					}
				}

				if (!bFoundNext)
				{
					// Backtrack - pop current and add parent back to path
					Stack.Pop();
					if (!Stack.IsEmpty())
					{
						WalkNodes.Add(Stack.Last().Key);
					}
				}
			}

			if (WalkNodes.Num() >= 3)
			{
				// Build wrapper cell from tree walk
				WrapperCell = MakeShared<FCell>(TempConstraints.ToSharedRef());
				WrapperCell->Nodes = MoveTemp(WalkNodes);
				WrapperCell->Polygon.Reserve(WrapperCell->Nodes.Num());
				WrapperCell->Data.Bounds = FBox(ForceInit);
				WrapperCell->Data.Centroid = FVector::ZeroVector;

				TSet<int32> UniqueNodes;
				for (const int32 NodeIdx : WrapperCell->Nodes)
				{
					const int32 PointIdx = Nodes[NodeIdx].PointIndex;
					WrapperCell->Polygon.Add(ProjectedPositions[PointIdx]);

					if (!UniqueNodes.Contains(NodeIdx))
					{
						UniqueNodes.Add(NodeIdx);
						const FVector Pos = InCluster->GetPos(NodeIdx);
						WrapperCell->Data.Bounds += Pos;
						WrapperCell->Data.Centroid += Pos;
					}
				}

				WrapperCell->Data.Centroid /= UniqueNodes.Num();
				WrapperCell->Data.bIsClosedLoop = true;
				WrapperCell->Data.bIsConvex = false; // Tree wrap-around is never convex

				// Compute perimeter (sum of all edge traversals)
				double Perimeter = 0;
				for (int32 i = 0; i < WrapperCell->Polygon.Num() - 1; ++i)
				{
					Perimeter += FVector2D::Distance(WrapperCell->Polygon[i], WrapperCell->Polygon[i + 1]);
				}
				WrapperCell->Data.Perimeter = Perimeter;

				// Area for tree wrap-around is 0 (degenerate polygon)
				WrapperCell->Data.Area = 0;
				WrapperCell->Data.Compactness = 0;

				WrapperCell->bBuiltSuccessfully = true;
			}
		}

		if (WrapperCell)
		{
			IsUniqueCellHash(WrapperCell);
		}
	}

	void FCellConstraints::Cleanup()
	{
		WrapperCell = nullptr;
	}

	uint64 FCell::GetCellHash()
	{
		if (CellHash != 0) { return CellHash; }
		CellHash = CityHash64(reinterpret_cast<const char*>(Nodes.GetData()), Nodes.Num() * sizeof(int32));
		return CellHash;
	}

	void FCell::PostProcessPoints(UPCGBasePointData* InMutablePoints)
	{
	}
}
