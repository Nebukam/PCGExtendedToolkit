// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Clusters/Artifacts/PCGExPlanarFaceEnumerator.h"

#include "Clusters/PCGExCluster.h"
#include "Clusters/Artifacts/PCGExCell.h"
#include "Math/PCGExMath.h"
#include "Math/Geo/PCGExGeo.h"

namespace PCGExClusters
{
	void FPlanarFaceEnumerator::Build(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& InProjectedPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPlanarFaceEnumerator::Build);

		Cluster = &InCluster.Get();
		ProjectedPositions = &InProjectedPositions;

		const TArray<FNode>& Nodes = *Cluster->Nodes;
		const TArray<PCGExGraphs::FEdge>& Edges = *Cluster->Edges;
		PCGEx::FIndexLookup* NodeLookup = Cluster->NodeIndexLookup.Get();
		const int32 NumEdges = Edges.Num();
		const int32 NumNodes = Nodes.Num();

		// Step 1: Create all half-edges (2 per edge)
		HalfEdges.Reset();
		HalfEdges.Reserve(NumEdges * 2);
		HalfEdgeMap.Reset();
		HalfEdgeMap.Reserve(NumEdges * 2);

		for (int32 EdgeIdx = 0; EdgeIdx < NumEdges; ++EdgeIdx)
		{
			const PCGExGraphs::FEdge& Edge = Edges[EdgeIdx];
			// Edge.Start and Edge.End are POINT indices, convert to node indices
			const int32 NodeA = NodeLookup->Get(Edge.Start);
			const int32 NodeB = NodeLookup->Get(Edge.End);

			// Get 2D positions using point indices directly
			const FVector2D PosA = InProjectedPositions[Edge.Start];
			const FVector2D PosB = InProjectedPositions[Edge.End];

			// Half-edge A → B
			const FVector2D DirAB = (PosB - PosA).GetSafeNormal();
			const double AngleAB = FMath::Atan2(DirAB.Y, DirAB.X);
			const int32 IndexAB = HalfEdges.Num();
			HalfEdges.Emplace(NodeA, NodeB, AngleAB);
			HalfEdgeMap.Add(PCGEx::H64(NodeA, NodeB), IndexAB);

			// Half-edge B → A
			const FVector2D DirBA = (PosA - PosB).GetSafeNormal();
			const double AngleBA = FMath::Atan2(DirBA.Y, DirBA.X);
			const int32 IndexBA = HalfEdges.Num();
			HalfEdges.Emplace(NodeB, NodeA, AngleBA);
			HalfEdgeMap.Add(PCGEx::H64(NodeB, NodeA), IndexBA);

			// Link twins
			HalfEdges[IndexAB].TwinIndex = IndexBA;
			HalfEdges[IndexBA].TwinIndex = IndexAB;
		}

		// Step 2: For each vertex, collect and sort outgoing half-edges by angle
		// Then link the "next" pointers
		TArray<TArray<int32>> OutgoingByNode;
		OutgoingByNode.SetNum(NumNodes);

		// Collect outgoing half-edges for each node
		for (int32 HEIdx = 0; HEIdx < HalfEdges.Num(); ++HEIdx)
		{
			const int32 Origin = HalfEdges[HEIdx].OriginNode;
			OutgoingByNode[Origin].Add(HEIdx);
		}

		// Sort each node's outgoing half-edges by angle (CCW order)
		for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
		{
			TArray<int32>& Outgoing = OutgoingByNode[NodeIdx];
			if (Outgoing.Num() <= 1) { continue; }

			// Sort by angle (ascending = CCW order)
			Outgoing.Sort([this](const int32 A, const int32 B)
			{
				return HalfEdges[A].Angle < HalfEdges[B].Angle;
			});
		}

		// Step 3: Link "next" pointers
		// For half-edge (u → v), its "next" is the half-edge that comes after (v → u) in CCW order around v
		for (int32 HEIdx = 0; HEIdx < HalfEdges.Num(); ++HEIdx)
		{
			FHalfEdge& HE = HalfEdges[HEIdx];
			const int32 TargetNode = HE.TargetNode;
			const int32 TwinIdx = HE.TwinIndex;

			// Find where the twin (v → u) appears in v's sorted outgoing list
			const TArray<int32>& TargetOutgoing = OutgoingByNode[TargetNode];
			const int32 TwinPosInList = TargetOutgoing.Find(TwinIdx);

			if (TwinPosInList == INDEX_NONE)
			{
				// Should never happen in a valid graph
				HE.NextIndex = -1;
				continue;
			}

			// The "next" half-edge is the one AFTER the twin in CCW order
			// This gives us faces with interior on the LEFT (CCW traversal)
			const int32 NextPosInList = (TwinPosInList + 1) % TargetOutgoing.Num();
			HE.NextIndex = TargetOutgoing[NextPosInList];
		}

		NumFaces = 0;
	}

	void FPlanarFaceEnumerator::EnumerateAllFaces(TArray<TSharedPtr<FCell>>& OutCells, const TSharedRef<FCellConstraints>& Constraints, TArray<TSharedPtr<FCell>>* OutFailedCells)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPlanarFaceEnumerator::EnumerateAllFaces);

		if (!IsBuilt()) { return; }

		// Mark all half-edges as unvisited
		TArray<bool> Visited;
		Visited.SetNumZeroed(HalfEdges.Num());

		TArray<int32> FaceNodes;
		FaceNodes.Reserve(64);

		NumFaces = 0;

		// Enumerate faces by following "next" pointers
		for (int32 StartHE = 0; StartHE < HalfEdges.Num(); ++StartHE)
		{
			if (Visited[StartHE]) { continue; }

			// Trace the face starting from this half-edge
			FaceNodes.Reset();
			int32 CurrentHE = StartHE;
			const int32 MaxSteps = HalfEdges.Num(); // Failsafe

			for (int32 Step = 0; Step < MaxSteps; ++Step)
			{
				if (CurrentHE < 0 || CurrentHE >= HalfEdges.Num())
				{
					// Invalid index - malformed DCEL
					FaceNodes.Reset();
					break;
				}

				if (Visited[CurrentHE])
				{
					// We've completed the face (or hit an already-visited edge)
					if (CurrentHE == StartHE)
					{
						// Successfully closed the face
						break;
					}
					else
					{
						// Hit a different visited edge - malformed
						FaceNodes.Reset();
						break;
					}
				}

				Visited[CurrentHE] = true;
				FaceNodes.Add(HalfEdges[CurrentHE].OriginNode);
				HalfEdges[CurrentHE].FaceIndex = NumFaces;

				CurrentHE = HalfEdges[CurrentHE].NextIndex;
			}

			// Validate and create cell
			if (FaceNodes.Num() >= 3)
			{
				TSharedPtr<FCell> Cell = MakeShared<FCell>(Constraints);
				const ECellResult Result = BuildCellFromFace(FaceNodes, Cell, Constraints);

				if (Result == ECellResult::Success)
				{
					OutCells.Add(Cell);
				}
				else if (OutFailedCells && !Cell->Polygon.IsEmpty())
				{
					// Cell failed constraints but has valid polygon - useful for containment testing
					OutFailedCells->Add(Cell);
				}

				NumFaces++;
			}
		}
	}

	ECellResult FPlanarFaceEnumerator::BuildCellFromFace(
		const TArray<int32>& FaceNodes,
		TSharedPtr<FCell>& OutCell,
		const TSharedRef<FCellConstraints>& Constraints) const
	{
		const int32 NumUniqueNodes = FaceNodes.Num();
		if (NumUniqueNodes < 3) { return ECellResult::Leaf; }

		// Check point count limits (based on unique nodes)
		if (NumUniqueNodes < Constraints->MinPointCount || NumUniqueNodes > Constraints->MaxPointCount)
		{
			return ECellResult::OutsidePointsLimit;
		}

		// Build nodes array with leaf duplication support
		OutCell->Nodes.Reset();
		OutCell->Nodes.Reserve(NumUniqueNodes * 2); // Reserve extra for potential leaf duplicates

		OutCell->Data.Bounds = FBox(ForceInit);
		OutCell->Data.Centroid = FVector::ZeroVector;

		double Perimeter = 0;
		int32 Sign = 0;
		FVector PrevPos = Cluster->GetPos(FaceNodes.Last());

		for (int32 i = 0; i < NumUniqueNodes; ++i)
		{
			const int32 NodeIdx = FaceNodes[i];
			const FNode& Node = (*Cluster->Nodes)[NodeIdx];
			const bool bIsLeaf = Node.IsLeaf();

			// Check for leaves
			if (bIsLeaf && !Constraints->bKeepCellsWithLeaves)
			{
				return ECellResult::Leaf;
			}

			// Add node (and duplicate if leaf and duplication is enabled)
			OutCell->Nodes.Add(NodeIdx);
			if (bIsLeaf && Constraints->bDuplicateLeafPoints)
			{
				OutCell->Nodes.Add(NodeIdx);
			}

			const FVector Pos = Cluster->GetPos(NodeIdx);

			OutCell->Data.Bounds += Pos;
			OutCell->Data.Centroid += Pos;

			// Compute segment length
			const double SegmentLength = FVector::Dist(PrevPos, Pos);
			Perimeter += SegmentLength;
			PrevPos = Pos;

			if (SegmentLength < Constraints->MinSegmentLength || SegmentLength > Constraints->MaxSegmentLength)
			{
				return ECellResult::OutsideSegmentsLimit;
			}

			// Check convexity
			if (i >= 2)
			{
				PCGExMath::CheckConvex(
					Cluster->GetPos(FaceNodes[i - 2]),
					Cluster->GetPos(FaceNodes[i - 1]),
					Pos,
					OutCell->Data.bIsConvex,
					Sign);

				if (Constraints->bConvexOnly && !OutCell->Data.bIsConvex)
				{
					return ECellResult::WrongAspect;
				}
			}
		}

		// Normalize nodes for hash computation
		PCGExArrayHelpers::ShiftArrayToSmallest(OutCell->Nodes);

		// Check for duplicate
		if (!Constraints->IsUniqueCellHash(OutCell)) { return ECellResult::Duplicate; }

		OutCell->Data.Centroid /= NumUniqueNodes;
		OutCell->Data.Perimeter = Perimeter;
		OutCell->Data.bIsClosedLoop = true;

		// Check bounds size
		const double BoundsSize = OutCell->Data.Bounds.GetSize().Length();
		if (BoundsSize < Constraints->MinBoundsSize || BoundsSize > Constraints->MaxBoundsSize)
		{
			return ECellResult::OutsideBoundsLimit;
		}

		// Check perimeter limits
		if (Perimeter < Constraints->MinPerimeter || Perimeter > Constraints->MaxPerimeter)
		{
			return ECellResult::OutsidePerimeterLimit;
		}

		// Build polygon from the expanded nodes array (includes leaf duplicates)
		const int32 NumOutputNodes = OutCell->Nodes.Num();
		OutCell->Polygon.SetNumUninitialized(NumOutputNodes);
		for (int32 i = 0; i < NumOutputNodes; ++i)
		{
			const int32 NodeIdx = OutCell->Nodes[i];
			const FNode& Node = (*Cluster->Nodes)[NodeIdx];
			OutCell->Polygon[i] = (*ProjectedPositions)[Node.PointIndex];
		}

		// Compute polygon properties (area, winding, compactness)
		PCGExMath::FPolygonInfos PolyInfos = PCGExMath::FPolygonInfos(OutCell->Polygon);
		OutCell->Data.Area = PolyInfos.Area * 0.01; // QoL scaling
		OutCell->Data.bIsClockwise = PolyInfos.bIsClockwise;
		OutCell->Data.Compactness = PolyInfos.Compactness;

		// Fix winding if needed
		if (!PolyInfos.IsWinded(Constraints->Winding))
		{
			Algo::Reverse(OutCell->Nodes);
			Algo::Reverse(OutCell->Polygon);
		}

		// Check holes
		if (Constraints->Holes && Constraints->Holes->Overlaps(OutCell->Polygon))
		{
			return ECellResult::Hole;
		}

		// Check compactness limits
		if (OutCell->Data.Compactness < Constraints->MinCompactness ||
			OutCell->Data.Compactness > Constraints->MaxCompactness)
		{
			return ECellResult::OutsideCompactnessLimit;
		}

		// Check area limits
		if (OutCell->Data.Area < Constraints->MinArea || OutCell->Data.Area > Constraints->MaxArea)
		{
			return ECellResult::OutsideAreaLimit;
		}

		// Check concave constraint
		if (Constraints->bConcaveOnly && OutCell->Data.bIsConvex)
		{
			return ECellResult::WrongAspect;
		}

		// Check wrapper cell match
		if (Constraints->WrapperCell && Constraints->WrapperClassificationTolerance > 0 &&
			FMath::IsNearlyEqual(OutCell->Data.Area, Constraints->WrapperCell->Data.Area, Constraints->WrapperClassificationTolerance))
		{
			return ECellResult::WrapperCell;
		}

		OutCell->bBuiltSuccessfully = true;
		return ECellResult::Success;
	}

	int32 FPlanarFaceEnumerator::FindFaceContaining(const FVector2D& Point) const
	{
		// Simple point-in-polygon test for each face
		// This could be optimized with spatial indexing
		TArray<FVector2D> FacePolygon;

		for (int32 StartHE = 0; StartHE < HalfEdges.Num(); ++StartHE)
		{
			const int32 FaceIdx = HalfEdges[StartHE].FaceIndex;
			if (FaceIdx < 0) { continue; }

			// Check if we've already tested this face
			bool bAlreadyTested = false;
			for (int32 PrevHE = 0; PrevHE < StartHE; ++PrevHE)
			{
				if (HalfEdges[PrevHE].FaceIndex == FaceIdx)
				{
					bAlreadyTested = true;
					break;
				}
			}
			if (bAlreadyTested) { continue; }

			// Build face polygon
			FacePolygon.Reset();
			int32 CurrentHE = StartHE;
			const int32 MaxSteps = HalfEdges.Num();

			for (int32 Step = 0; Step < MaxSteps; ++Step)
			{
				const FHalfEdge& HE = HalfEdges[CurrentHE];
				FacePolygon.Add((*ProjectedPositions)[(*Cluster->Nodes)[HE.OriginNode].PointIndex]);
				CurrentHE = HE.NextIndex;
				if (CurrentHE == StartHE) { break; }
			}

			if (FacePolygon.Num() >= 3 && PCGExMath::Geo::IsPointInPolygon(Point, FacePolygon))
			{
				return FaceIdx;
			}
		}

		return -1;
	}

	int32 FPlanarFaceEnumerator::GetWrapperFaceIndex() const
	{
		// The wrapper face is the one with the largest (most negative for CCW) signed area
		// or equivalently the face that would have CW winding when all others have CCW
		double LargestArea = -MAX_dbl;
		int32 WrapperIdx = -1;

		TArray<FVector2D> FacePolygon;
		TSet<int32> ProcessedFaces;

		for (int32 StartHE = 0; StartHE < HalfEdges.Num(); ++StartHE)
		{
			const int32 FaceIdx = HalfEdges[StartHE].FaceIndex;
			if (FaceIdx < 0 || ProcessedFaces.Contains(FaceIdx)) { continue; }
			ProcessedFaces.Add(FaceIdx);

			// Build face polygon
			FacePolygon.Reset();
			int32 CurrentHE = StartHE;
			const int32 MaxSteps = HalfEdges.Num();

			for (int32 Step = 0; Step < MaxSteps; ++Step)
			{
				const FHalfEdge& HE = HalfEdges[CurrentHE];
				FacePolygon.Add((*ProjectedPositions)[(*Cluster->Nodes)[HE.OriginNode].PointIndex]);
				CurrentHE = HE.NextIndex;
				if (CurrentHE == StartHE) { break; }
			}

			if (FacePolygon.Num() >= 3)
			{
				// Compute signed area - wrapper will have opposite sign
				double SignedArea = 0;
				for (int32 i = 0; i < FacePolygon.Num(); ++i)
				{
					const FVector2D& P1 = FacePolygon[i];
					const FVector2D& P2 = FacePolygon[(i + 1) % FacePolygon.Num()];
					SignedArea += (P1.X * P2.Y - P2.X * P1.Y);
				}
				SignedArea *= 0.5;

				// The wrapper face will have the largest absolute area
				const double AbsArea = FMath::Abs(SignedArea);
				if (AbsArea > LargestArea)
				{
					LargestArea = AbsArea;
					WrapperIdx = FaceIdx;
				}
			}
		}

		return WrapperIdx;
	}
}
