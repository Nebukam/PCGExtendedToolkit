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
#pragma region DCEL Face Enumeration

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

	void FPlanarFaceEnumerator::EnumerateAllFaces(TArray<TSharedPtr<FCell>>& OutCells, const TSharedRef<FCellConstraints>& Constraints)
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

				NumFaces++;
			}
		}
	}

	ECellResult FPlanarFaceEnumerator::BuildCellFromFace(
		const TArray<int32>& FaceNodes,
		TSharedPtr<FCell>& OutCell,
		const TSharedRef<FCellConstraints>& Constraints) const
	{
		const int32 NumNodes = FaceNodes.Num();
		if (NumNodes < 3) { return ECellResult::Leaf; }

		// Copy nodes and normalize for hash computation
		OutCell->Nodes = FaceNodes;
		PCGExArrayHelpers::ShiftArrayToSmallest(OutCell->Nodes);

		// Check for duplicate
		if (!Constraints->IsUniqueCellHash(OutCell)) { return ECellResult::Duplicate; }

		// Check point count limits
		if (NumNodes < Constraints->MinPointCount || NumNodes > Constraints->MaxPointCount)
		{
			return ECellResult::OutsidePointsLimit;
		}

		// Build polygon and compute properties
		OutCell->Polygon.SetNumUninitialized(NumNodes);
		OutCell->Data.Bounds = FBox(ForceInit);
		OutCell->Data.Centroid = FVector::ZeroVector;

		double Perimeter = 0;
		int32 Sign = 0;
		FVector PrevPos = Cluster->GetPos(FaceNodes.Last());

		for (int32 i = 0; i < NumNodes; ++i)
		{
			const int32 NodeIdx = FaceNodes[i];
			const FNode& Node = (*Cluster->Nodes)[NodeIdx];

			// Check for leaves
			if (Node.IsLeaf() && !Constraints->bKeepCellsWithLeaves)
			{
				return ECellResult::Leaf;
			}

			const FVector Pos = Cluster->GetPos(NodeIdx);
			OutCell->Polygon[i] = (*ProjectedPositions)[Node.PointIndex];

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

		OutCell->Data.Centroid /= NumNodes;
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

#pragma endregion

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
		TempConstraints->bDuplicateLeafPoints = false;

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

	ECellResult FCell::BuildFromCluster(const PCGExGraphs::FLink InSeedLink, TSharedRef<FCluster> InCluster, const TArray<FVector2D>& ProjectedPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCell::BuildFromCluster);

		ON_SCOPE_EXIT
		{
			Nodes.Shrink();
		};

		bBuiltSuccessfully = false;
		Data.Bounds = FBox(ForceInit);

		Seed = InSeedLink;
		// From node, through edge; edge will be updated to be last traversed after.
		PCGExGraphs::FLink From = InSeedLink;
		// To node, through edge
		PCGExGraphs::FLink To = PCGExGraphs::FLink(InCluster->GetEdgeOtherNode(From)->Index, Seed.Edge);

		const uint64 SeedHalfEdge = PCGEx::H64(From.Node, To.Node);
		if (!Constraints->IsUniqueStartHalfEdge(SeedHalfEdge)) { return ECellResult::Duplicate; }

		const FVector SeedRP = InCluster->GetPos(From.Node);

		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(SeedRP);
		Data.Centroid = SeedRP;
		Data.Bounds += SeedRP;

		Nodes.Reserve(32);
		Nodes.Add(From.Node);
		if (InCluster->GetNode(From.Node)->IsLeaf() && Constraints->bDuplicateLeafPoints) { Nodes.Add(From.Node); }

		int32 NumUniqueNodes = 1;

		const int32 FailSafe = InCluster->Edges->Num() * 2;

		TSet<uint64> SignedEdges;
		SignedEdges.Reserve(32);

		while (To.Node != -1)
		{
			if (SignedEdges.Num() > FailSafe) { return ECellResult::MalformedCluster; }

			bool bEdgeAlreadyExists;
			uint64 SignedEdgeHash = PCGEx::H64(From.Node, To.Node);

			SignedEdges.Add(SignedEdgeHash, &bEdgeAlreadyExists);
			if (bEdgeAlreadyExists)
			{
				if (To.Edge == Seed.Edge || To.Node == Seed.Node)
				{
					Data.bIsClosedLoop = true;
					const int32 RemovedIndex = Nodes.Pop();
					if (RemovedIndex == Nodes.Last()) { Nodes.Pop(); }
					break;
				}

				return ECellResult::OpenCell;
			}

			const FNode* Current = InCluster->GetNode(To.Node);

			Nodes.Add(Current->Index);
			NumUniqueNodes++;

			const FVector& RP = InCluster->GetPos(Current);
			Data.Centroid += RP;

			double SegmentLength = 0;
			const double NewLength = Metrics.Add(RP, SegmentLength);
			if (NewLength > Constraints->MaxPerimeter) { return ECellResult::OutsidePerimeterLimit; }
			if (SegmentLength < Constraints->MinSegmentLength || SegmentLength > Constraints->MaxSegmentLength) { return ECellResult::OutsideSegmentsLimit; }

			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::OutsidePointsLimit; }

			Data.Bounds += RP;
			if (Data.Bounds.GetSize().Length() > Constraints->MaxBoundsSize) { return ECellResult::OutsideBoundsLimit; }

			int32 LockedEdge = To.Edge;

			if (Current->IsLeaf())
			{
				LockedEdge = -1;
				if (Constraints->bDuplicateLeafPoints) { Nodes.Add(Current->Index); }
			}

			// Seek next best candidate using DCEL-style angle calculation
			// At current vertex, find the outgoing edge that comes just AFTER the incoming edge's reverse in CCW order
			const FVector2D CurrentPos = ProjectedPositions[Current->PointIndex];
			const FVector2D FromPos = ProjectedPositions[InCluster->GetNodePointIndex(From.Node)];

			// The incoming edge's reverse direction: from current back to where we came from
			const FVector2D IncomingReverse = (FromPos - CurrentPos).GetSafeNormal();
			const double IncomingReverseAngle = FMath::Atan2(IncomingReverse.Y, IncomingReverse.X);

			From = To;
			To = PCGExGraphs::FLink(-1, -1);

			// Find the outgoing edge with smallest CCW angle from the incoming reverse
			double SmallestCCWAngle = MAX_dbl;
			for (const PCGExGraphs::FLink& Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (Lk.Edge == LockedEdge) { continue; }

				const FVector2D NeighborPos = ProjectedPositions[InCluster->GetNodePointIndex(NeighborIndex)];
				const FVector2D OutgoingDir = (NeighborPos - CurrentPos).GetSafeNormal();
				const double OutgoingAngle = FMath::Atan2(OutgoingDir.Y, OutgoingDir.X);

				// Compute CCW angle from incoming reverse to this outgoing edge
				// This gives us the angle we'd turn CCW from where we came from
				double CCWAngle = OutgoingAngle - IncomingReverseAngle;
				if (CCWAngle <= 0) { CCWAngle += TWO_PI; }  // Wrap to (0, 2π]

				if (CCWAngle < SmallestCCWAngle)
				{
					SmallestCCWAngle = CCWAngle;
					To = Lk;
				}
			}

			if (To.Node == -1) { return ECellResult::OpenCell; }

			if (InCluster->GetNode(To.Node)->Num() == 1 && !Constraints->bKeepCellsWithLeaves) { return ECellResult::Leaf; }
			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::OutsideBoundsLimit; }

			if (NumUniqueNodes > 2)
			{
				PCGExMath::CheckConvex(InCluster->GetPos(Nodes.Last(2)), InCluster->GetPos(Nodes.Last(1)), InCluster->GetPos(Nodes.Last()), Data.bIsConvex, Sign);

				if (Constraints->bConvexOnly && !Data.bIsConvex) { return ECellResult::WrongAspect; }
			}
		}

		if (NumUniqueNodes <= 2) { return ECellResult::Leaf; }

		if (!Data.bIsClosedLoop) { return ECellResult::OpenCell; }

		PCGExArrayHelpers::ShiftArrayToSmallest(Nodes);

		if (!Constraints->IsUniqueCellHash(SharedThis(this))) { return ECellResult::Duplicate; }

		bBuiltSuccessfully = true;

		Data.Centroid /= NumUniqueNodes;

		Data.Perimeter = Metrics.Length;
		const double LastSegmentLength = FVector::Dist(InCluster->GetPos(Nodes[0]), InCluster->GetPos(Nodes.Last()));
		if (Constraints->MinSegmentLength > LastSegmentLength || LastSegmentLength > Constraints->MaxSegmentLength) { return ECellResult::OutsideSegmentsLimit; }

		if (Data.Perimeter < Constraints->MinPerimeter || Data.Perimeter > Constraints->MaxPerimeter) { return ECellResult::OutsidePerimeterLimit; }

		if (Constraints->bConcaveOnly && Data.bIsConvex) { return ECellResult::WrongAspect; }
		if (NumUniqueNodes < Constraints->MinPointCount) { return ECellResult::OutsidePointsLimit; }
		if (Data.Bounds.GetSize().Length() < Constraints->MinBoundsSize) { return ECellResult::OutsideBoundsLimit; }

		Polygon.SetNumUninitialized(Nodes.Num());
		for (int i = 0; i < Nodes.Num(); ++i) { Polygon[i] = FVector2D(ProjectedPositions[InCluster->GetNodePointIndex(Nodes[i])]); }

		PCGExMath::FPolygonInfos PolyInfos = PCGExMath::FPolygonInfos(Polygon);

		Data.Area = PolyInfos.Area;
		Data.bIsClockwise = PolyInfos.bIsClockwise;
		Data.Compactness = PolyInfos.Compactness;

		if (!PolyInfos.IsWinded(Constraints->Winding))
		{
			Algo::Reverse(Nodes);
			Algo::Reverse(Polygon);
		}

		if (Constraints->Holes && Constraints->Holes->Overlaps(Polygon)) { return ECellResult::Hole; }
		if (Constraints->MinCompactness > Data.Compactness || Data.Compactness > Constraints->MaxCompactness) { return ECellResult::OutsideCompactnessLimit; }

		Data.Area *= 0.01;
		if (Constraints->MinArea > Data.Area || Data.Area > Constraints->MaxArea) { return ECellResult::OutsideAreaLimit; }

		if (Constraints->WrapperCell && Constraints->WrapperClassificationTolerance > 0 && FMath::IsNearlyEqual(Data.Area, Constraints->WrapperCell->Data.Area, Constraints->WrapperClassificationTolerance))
		{
			return ECellResult::WrapperCell;
		}

		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromCluster(const FVector& SeedPosition, const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const FPCGExGeo2DProjectionDetails& ProjectionDetails, const FPCGExNodeSelectionDetails* Picking)
	{
		// Project the seed position to 2D
		const FVector ProjectedSeed3D = ProjectionDetails.ProjectFlat(SeedPosition);
		const FVector2D Seed2D(ProjectedSeed3D.X, ProjectedSeed3D.Y);

		// Find the globally closest edge in 3D world space using EdgeOctree
		double BestDistSq = MAX_dbl;
		int32 BestEdgeIdx = -1;

		InCluster->GetEdgeOctree()->FindNearbyElements(SeedPosition, [&](const PCGExOctree::FItem& Item)
		{
			const double Dist = InCluster->GetPointDistToEdgeSquared(Item.Index, SeedPosition);
			if (Dist < BestDistSq)
			{
				BestDistSq = Dist;
				BestEdgeIdx = Item.Index;
			}
		});

		if (BestEdgeIdx == -1)
		{
			return ECellResult::Unknown;
		}

		// Optional: Apply picking distance constraint
		if (Picking)
		{
			const FEdge* Edge = InCluster->GetEdge(BestEdgeIdx);
			const FVector EdgeStart3D = InCluster->GetStartPos(Edge);
			const FVector EdgeEnd3D = InCluster->GetEndPos(Edge);
			const FVector ClosestOnEdge = FMath::ClosestPointOnSegment(SeedPosition, EdgeStart3D, EdgeEnd3D);
			if (!Picking->WithinDistance(ClosestOnEdge, SeedPosition))
			{
				return ECellResult::Unknown;
			}
		}

		// Get edge endpoints
		const FEdge* Edge = InCluster->GetEdge(BestEdgeIdx);
		const int32 StartPointIdx = Edge->Start;
		const int32 EndPointIdx = Edge->End;
		const int32 StartNodeIdx = InCluster->NodeIndexLookup->Get(StartPointIdx);
		const int32 EndNodeIdx = InCluster->NodeIndexLookup->Get(EndPointIdx);

		// Determine which side of the edge the seed is on using 2D perp dot
		const FVector2D& EdgeStart2D = ProjectedPositions[StartPointIdx];
		const FVector2D& EdgeEnd2D = ProjectedPositions[EndPointIdx];
		const FVector2D EdgeDir = EdgeEnd2D - EdgeStart2D;
		const FVector2D SeedDir = Seed2D - EdgeStart2D;
		const double PerpDot = EdgeDir.X * SeedDir.Y - EdgeDir.Y * SeedDir.X;

		// Choose starting node based on which side the seed is on
		// PerpDot > 0 means seed is on LEFT of edge (Start→End direction)
		// We want to trace the face that's on the SAME side as the seed
		PCGExGraphs::FLink Link;
		Link.Edge = BestEdgeIdx;
		Link.Node = (PerpDot >= 0) ? EndNodeIdx : StartNodeIdx;

		return BuildFromCluster(Link, InCluster, ProjectedPositions);
	}

	ECellResult FCell::BuildFromPath(const TArray<FVector2D>& ProjectedPositions)
	{
		return ECellResult::Unknown;
	}

	void FCell::PostProcessPoints(UPCGBasePointData* InMutablePoints)
	{
	}
}
