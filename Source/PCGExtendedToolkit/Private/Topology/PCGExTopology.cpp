// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Topology/PCGExTopology.h"

#include "PCGExContext.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointElements.h"
#include "Data/PCGExPointIO.h"
#include "Geometry/PCGExGeoPrimtives.h"
#include "Graph/PCGExCluster.h"
#include "Paths/PCGExPaths.h"
#include "GeometryScript/PolygonFunctions.h"

void FPCGExCellSeedMutationDetails::ApplyToPoint(const PCGExTopology::FCell* InCell, PCGExData::FMutablePoint& OutSeedPoint, const UPCGBasePointData* CellPoints) const
{
	switch (Location)
	{
	default:
	case EPCGExCellSeedLocation::Original:
		break;
	case EPCGExCellSeedLocation::Centroid:
		OutSeedPoint.SetLocation(InCell->Data.Centroid);
		break;
	case EPCGExCellSeedLocation::PathBoundsCenter:
		OutSeedPoint.SetLocation(InCell->Data.Bounds.GetCenter());
		break;
	case EPCGExCellSeedLocation::FirstNode:
		OutSeedPoint.SetLocation(CellPoints->GetTransform(0).GetLocation());
		break;
	case EPCGExCellSeedLocation::LastNode:
		OutSeedPoint.SetLocation(CellPoints->GetTransform(CellPoints->GetNumPoints() - 1).GetLocation());
		break;
	}

	if (bResetScale) { OutSeedPoint.SetScale3D(FVector::OneVector); }

	if (bResetRotation) { OutSeedPoint.SetRotation(FQuat::Identity); }

	if (bMatchCellBounds)
	{
		const FVector Offset = OutSeedPoint.GetLocation();
		OutSeedPoint.SetBoundsMin(InCell->Data.Bounds.Min - Offset);
		OutSeedPoint.SetBoundsMax(InCell->Data.Bounds.Max - Offset);
	}

	PCGExTopology::SetPointProperty(OutSeedPoint, InCell->Data.Area, AreaTo);
	PCGExTopology::SetPointProperty(OutSeedPoint, InCell->Data.Perimeter, PerimeterTo);
	PCGExTopology::SetPointProperty(OutSeedPoint, InCell->Data.Compactness, CompactnessTo);
}

void FPCGExTopologyDetails::PostProcessMesh(const TObjectPtr<UDynamicMesh>& InDynamicMesh) const
{
	if (bWeldEdges) { UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(InDynamicMesh, WeldEdgesOptions); }
	if (bComputeNormals) { UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(InDynamicMesh, NormalsOptions); }
}

namespace PCGExTopology
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

	bool IsAnyPointInPolygon(const TArray<FVector2D>& Points, const FGeometryScriptSimplePolygon& Polygon)
	{
		if (Points.IsEmpty()) { return false; }
		const TArray<FVector2D>& Vertices = *Polygon.Vertices;
		for (const FVector2D& P : Points) { if (FGeomTools2D::IsPointInPolygon(P, Vertices)) { return true; } }
		return false;
	}

	bool IsPolygonInPolygon(const FGeometryScriptSimplePolygon& ContainerPolygon, const FGeometryScriptSimplePolygon& Polygon)
	{
		const TArray<FVector2D>& ContainerPoints = *ContainerPolygon.Vertices;
		const TArray<FVector2D>& PolyPoints = *Polygon.Vertices;
		for (const FVector2D& Point : PolyPoints)
		{
			if (!FGeomTools2D::IsPointInPolygon(Point, ContainerPoints)) { return false; }
		}
		return true;
	}

	void MarkTriangle(const TSharedPtr<PCGExCluster::FCluster>& InCluster, const PCGExGeo::FTriangle& InTriangle)
	{
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[0])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[1])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[2])->bValid, 1);
	}

	bool FHoles::Overlaps(const FGeometryScriptSimplePolygon& Polygon)
	{
		{
			FReadScopeLock ReadScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return IsAnyPointInPolygon(ProjectedPoints, Polygon); }
		}

		{
			FWriteScopeLock WriteScopeLock(ProjectionLock);
			if (!ProjectedPoints.IsEmpty()) { return IsAnyPointInPolygon(ProjectedPoints, Polygon); }

			ProjectionDetails.ProjectFlat(PointDataFacade, ProjectedPoints);
			return IsAnyPointInPolygon(ProjectedPoints, Polygon);
		}
	}

	bool FCellConstraints::ContainsSignedEdgeHash(const uint64 Hash) const
	{
		FReadScopeLock ReadScopeLock(UniqueStartHalfEdgesHashLock);
		return UniqueStartHalfEdgesHash.Contains(Hash);
	}

	bool FCellConstraints::IsUniqueStartHalfEdge(const uint64 Hash)
	{
		bool bAlreadyExists = false;
		{
			FReadScopeLock ReadScopeLock(UniqueStartHalfEdgesHashLock);
			bAlreadyExists = UniqueStartHalfEdgesHash.Contains(Hash);
			if (bAlreadyExists) { return false; }
		}
		{
			FWriteScopeLock WriteScopeLock(UniqueStartHalfEdgesHashLock);
			UniqueStartHalfEdgesHash.Add(Hash, &bAlreadyExists);
			return !bAlreadyExists;
		}
	}

	bool FCellConstraints::IsUniqueCellHash(const TSharedPtr<FCell>& InCell)
	{
		const uint32 CellHash = InCell->GetCellHash();

		{
			FReadScopeLock ReadScopeLock(UniqueStartHalfEdgesHashLock);
			if (UniquePathsHashSet.Contains(CellHash)) { return false; }
		}

		{
			FWriteScopeLock WriteScope(UniqueStartHalfEdgesHashLock);
			bool bAlreadyExists;
			UniquePathsHashSet.Add(CellHash, &bAlreadyExists);
			return !bAlreadyExists;
		}
	}

	void FCellConstraints::BuildWrapperCell(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const TSharedPtr<FCellConstraints>& InConstraints)
	{
		double MaxDist = 0;
		PCGExGraph::FLink Link = PCGExGraph::FLink(-1, -1);
		const TArray<PCGExCluster::FNode>& Nodes = *InCluster->Nodes.Get();
		for (const PCGExCluster::FNode& Node : Nodes)
		{
			if (const double Dist = FVector2D::DistSquared(InCluster->ProjectedCentroid, ProjectedPositions[Node.PointIndex]);
				Dist > MaxDist)
			{
				Link.Node = Node.Index;
				MaxDist = Dist;
			}
		}

		if (Link.Node == -1)
		{
			WrapperCell = nullptr;
			return;
		}

		TSharedPtr<FCellConstraints> TempConstraints = InConstraints;
		if (!InConstraints)
		{
			TempConstraints = MakeShared<FCellConstraints>();
			TempConstraints->bKeepCellsWithLeaves = bKeepCellsWithLeaves;
			TempConstraints->bDuplicateLeafPoints = bDuplicateLeafPoints;
			TempConstraints->Winding = Winding;
		}

		// Find edge that's pointing away from the local center the most
		double BestDot = MAX_dbl;
		PCGExCluster::FNode* SeedNode = InCluster->GetNode(Link);
		const FVector2D From = ProjectedPositions[SeedNode->PointIndex];
		const FVector2D TowardCenter = (InCluster->ProjectedCentroid - From).GetSafeNormal();

		for (const PCGExGraph::FLink& Lk : SeedNode->Links)
		{
			double Dot = FVector2D::DotProduct(TowardCenter, (ProjectedPositions[InCluster->GetNodePointIndex(Lk)] - From).GetSafeNormal());
			if (Dot < BestDot)
			{
				BestDot = Dot;
				Link.Edge = Lk.Edge;
			}
		}

		if (Link.Edge == -1)
		{
			WrapperCell = nullptr;
			return;
		}

		auto GetGuidedHalfEdge = [&]()
		{
			PCGExCluster::FNode* StartNode = InCluster->GetEdgeStart(Link.Edge);
			PCGExCluster::FNode* EndNode = InCluster->GetEdgeEnd(Link.Edge);

			if (StartNode->IsLeaf() && !EndNode->IsLeaf()) { return StartNode; }
			if (EndNode->IsLeaf() && !StartNode->IsLeaf()) { return EndNode; }

			FVector2D EdgeDir = (ProjectedPositions[EndNode->PointIndex] - ProjectedPositions[StartNode->PointIndex]).GetSafeNormal();
			FVector2D Normal(-EdgeDir.Y, EdgeDir.X); // CCW normal

			if (FVector2D::DotProduct((ProjectedPositions[InCluster->GetNodePointIndex(Link)] - InCluster->ProjectedCentroid).GetSafeNormal(), Normal) > 0) { return StartNode; }
			return EndNode;
		};

		// Determine which node we should start with to be right-handed
		Link.Node = GetGuidedHalfEdge()->Index;

		if (const TSharedPtr<FCell> Cell = MakeShared<FCell>(TempConstraints.ToSharedRef());
			Cell->BuildFromCluster(Link, InCluster, ProjectedPositions) == ECellResult::Success)
		{
			WrapperCell = Cell;
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

	ECellResult FCell::BuildFromCluster(
		const PCGExGraph::FLink InSeedLink,
		TSharedRef<PCGExCluster::FCluster> InCluster,
		const TArray<FVector2D>& ProjectedPositions)
	{
		bBuiltSuccessfully = false;
		Data.Bounds = FBox(ForceInit);

		Seed = InSeedLink;
		// From node, through edge; edge will be updated to be last traversed after.
		PCGExGraph::FLink From = InSeedLink;
		// To node, through edge
		PCGExGraph::FLink To = PCGExGraph::FLink(InCluster->GetEdgeOtherNode(From)->Index, Seed.Edge);

		const uint64 SeedHalfEdge = PCGEx::H64(From.Node, To.Node);
		if (!Constraints->IsUniqueStartHalfEdge(SeedHalfEdge)) { return ECellResult::Duplicate; }

		const FVector SeedRP = InCluster->GetPos(From.Node);

		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(SeedRP);
		Data.Centroid = SeedRP;
		Data.Bounds += SeedRP;

		Nodes.Add(From.Node);
		if (InCluster->GetNode(From.Node)->IsLeaf() && Constraints->bDuplicateLeafPoints) { Nodes.Add(From.Node); }

		int32 NumUniqueNodes = 1;

		const int32 FailSafe = InCluster->Edges->Num() * 2;
		TSet<uint64> SignedEdges;
		while (To.Node != -1)
		{
			if (SignedEdges.Num() > FailSafe) { return ECellResult::MalformedCluster; } // Let's hope this never happens

			bool bEdgeAlreadyExists;
			uint64 SignedEdgeHash = PCGEx::H64(From.Node, To.Node);

			//if (SignedEdgeHash != UniqueStartEdgeHash && Constraints->ContainsSignedEdgeHash(SignedEdgeHash)) { return ECellResult::Duplicate; }

			SignedEdges.Add(SignedEdgeHash, &bEdgeAlreadyExists);
			if (bEdgeAlreadyExists)
			{
				if (To.Edge == Seed.Edge || To.Node == Seed.Node)
				{
					Data.bIsClosedLoop = true;
					const int32 RemovedIndex = Nodes.Pop();            // Remove last added point
					if (RemovedIndex == Nodes.Last()) { Nodes.Pop(); } // Remove last if duplicate (leaf)
					break;
				}

				return ECellResult::OpenCell;
			}

			// Add next node since it's valid

			const PCGExCluster::FNode* Current = InCluster->GetNode(To.Node);

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

			// Seek next best candidate
			const FVector2D PP = ProjectedPositions[Current->PointIndex];
			const FVector2D GuideDir = (PP - ProjectedPositions[InCluster->GetNodePointIndex(From.Node)]).GetSafeNormal();

			From = To;
			To = PCGExGraph::FLink(-1, -1);

			double BestAngle = MAX_dbl;
			for (const PCGExGraph::FLink Lk : Current->Links)
			{
				const int32 NeighborIndex = Lk.Node;

				if (Lk.Edge == LockedEdge) { continue; }

				const FVector2D OtherDir = (PP - ProjectedPositions[InCluster->GetNodePointIndex(NeighborIndex)]).GetSafeNormal();

				if (const double Angle = PCGExMath::GetRadiansBetweenVectors(OtherDir, GuideDir); Angle < BestAngle)
				{
					BestAngle = Angle;
					To = Lk;
				}
			}

			if (To.Node == -1) { return ECellResult::OpenCell; } // Failed to wrap

			if (InCluster->GetNode(To.Node)->Num() == 1 && !Constraints->bKeepCellsWithLeaves) { return ECellResult::Leaf; }
			if (NumUniqueNodes > Constraints->MaxPointCount) { return ECellResult::OutsideBoundsLimit; }

			if (NumUniqueNodes > 2)
			{
				PCGExMath::CheckConvex(
					InCluster->GetPos(Nodes.Last(2)),
					InCluster->GetPos(Nodes.Last(1)),
					InCluster->GetPos(Nodes.Last()),
					Data.bIsConvex, Sign);

				if (Constraints->bConvexOnly && !Data.bIsConvex) { return ECellResult::WrongAspect; }
			}
		}

		if (NumUniqueNodes <= 2) { return ECellResult::Leaf; }

		if (!Data.bIsClosedLoop) { return ECellResult::OpenCell; }

		PCGEx::ShiftArrayToSmallest(Nodes); // ! important to guarantee contour determinism

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

		Polygon.Reset();
		TArray<FVector2D>& Vertices = *Polygon.Vertices;
		Vertices.SetNumUninitialized(Nodes.Num());
		for (int i = 0; i < Nodes.Num(); ++i) { Vertices[i] = FVector2D(ProjectedPositions[InCluster->GetNodePointIndex(Nodes[i])]); }

		PCGExGeo::FPolygonInfos PolyInfos = PCGExGeo::FPolygonInfos(Vertices);

		Data.Area = PolyInfos.Area;
		Data.bIsClockwise = PolyInfos.bIsClockwise;
		Data.Compactness = PolyInfos.Compactness;

		if (!PolyInfos.IsWinded(Constraints->Winding))
		{
			Algo::Reverse(Nodes);
			Algo::Reverse(Vertices);
		}

		if (Constraints->Holes && Constraints->Holes->Overlaps(Polygon)) { return ECellResult::Hole; }
		if (Constraints->MinCompactness > Data.Compactness || Data.Compactness > Constraints->MaxCompactness) { return ECellResult::OutsideCompactnessLimit; }

		Data.Area *= 0.01; // QoL to avoid extra 000 in the detail panel.
		if (Constraints->MinArea > Data.Area || Data.Area > Constraints->MaxArea) { return ECellResult::OutsideAreaLimit; }

		if (Constraints->WrapperCell &&
			Constraints->WrapperClassificationTolerance > 0 &&
			FMath::IsNearlyEqual(Data.Area, Constraints->WrapperCell->Data.Area, Constraints->WrapperClassificationTolerance))
		{
			return ECellResult::WrapperCell;
		}

		return ECellResult::Success;
	}

	ECellResult FCell::BuildFromCluster(
		const FVector& SeedPosition,
		const TSharedRef<PCGExCluster::FCluster>& InCluster,
		const TArray<FVector2D>& ProjectedPositions,
		const FVector& UpVector,
		const FPCGExNodeSelectionDetails* Picking)
	{
		PCGExGraph::FLink Link = PCGExGraph::FLink(-1, -1);
		Link.Node = InCluster->FindClosestNode(SeedPosition, Picking ? Picking->PickingMethod : EPCGExClusterClosestSearchMode::Edge, 2);

		if (Link.Node == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		if (const FVector StartPosition = InCluster->GetPos(Link.Node);
			Picking && !Picking->WithinDistance(StartPosition, SeedPosition))
		{
			// Fail. Not within radius.
			return ECellResult::Unknown;
		}

		// Find edge closest to seed position
		Link.Edge = InCluster->FindClosestEdge(Link.Node, SeedPosition, 2);

		if (Link.Edge == -1)
		{
			// Fail. Either single-node or single-edge cluster, or no connected edge
			return ECellResult::Unknown;
		}

		const PCGExGraph::FEdge* E = InCluster->GetEdge(Link.Edge);

		// choose a deterministic right-hand frame		
		Link.Node = InCluster->GetGuidedHalfEdge(Link.Edge, SeedPosition, UpVector)->Index;
		return BuildFromCluster(Link, InCluster, ProjectedPositions);
	}

	ECellResult FCell::BuildFromPath(
		const TArray<FVector2D>& ProjectedPositions)
	{
		return ECellResult::Unknown;
	}

	void FCell::PostProcessPoints(UPCGBasePointData* InMutablePoints)
	{
	}
}


bool FPCGExCellArtifactsDetails::WriteAny() const
{
	return bWriteCellHash || bWriteArea || bWriteCompactness ||
		bWriteVtxId || bFlagTerminalPoint || bWriteNumRepeat;
}

bool FPCGExCellArtifactsDetails::Init(FPCGExContext* InContext)
{
	if (bWriteVtxId) { PCGEX_VALIDATE_NAME_C(InContext, VtxIdAttributeName); }
	if (bWriteCellHash) { PCGEX_VALIDATE_NAME_C(InContext, CellHashAttributeName); }
	if (bWriteArea) { PCGEX_VALIDATE_NAME_C(InContext, AreaAttributeName); }
	if (bWriteCompactness) { PCGEX_VALIDATE_NAME_C(InContext, CompactnessAttributeName); }
	if (bFlagTerminalPoint) { PCGEX_VALIDATE_NAME_C(InContext, TerminalFlagAttributeName); }
	if (bWriteNumRepeat) { PCGEX_VALIDATE_NAME_C(InContext, NumRepeatAttributeName); }
	TagForwarding.bFilterToRemove = true;
	TagForwarding.bPreservePCGExData = false;
	TagForwarding.Init();
	return true;
}

void FPCGExCellArtifactsDetails::Process(
	const TSharedPtr<PCGExCluster::FCluster>& InCluster,
	const TSharedPtr<PCGExData::FFacade>& InDataFacade,
	const TSharedPtr<PCGExTopology::FCell>& InCell) const
{
	auto FwdTags = [&](const TSet<FString>& SourceTags)
	{
		TArray<FString> Tags = SourceTags.Array();
		for (int i = 0; i < Tags.Num(); i++)
		{
			if (Tags[i].StartsWith(PCGExCommon::PCGExPrefix))
			{
				Tags.RemoveAt(i);
				i--;
			}
		}

		TagForwarding.Prune(Tags);
		InDataFacade->Source->Tags->Append(Tags);
	};

	// Lots of wasted cycles here
	FwdTags(InCluster->VtxIO.Pin()->Tags->Flatten());
	FwdTags(InCluster->EdgesIO.Pin()->Tags->Flatten());

	PCGExPaths::SetClosedLoop(InDataFacade->GetOut(), true);

	if (InCell->Data.bIsConvex) { if (bTagConvex) { InDataFacade->Source->Tags->AddRaw(ConvexTag); } }
	else { if (bTagConcave) { InDataFacade->Source->Tags->AddRaw(ConcaveTag); } }

	if (!WriteAny()) { return; }

	const int32 NumNodes = InCell->Nodes.Num();
	const TSharedPtr<PCGExData::TBuffer<bool>> TerminalBuffer = bFlagTerminalPoint ? InDataFacade->GetWritable(TerminalFlagAttributeName, false, true, PCGExData::EBufferInit::New) : nullptr;

	TMap<int32, int32> NumRepeats;
	const TSharedPtr<PCGExData::TBuffer<int32>> RepeatBuffer = bWriteNumRepeat ? InDataFacade->GetWritable(NumRepeatAttributeName, 0, true, PCGExData::EBufferInit::New) : nullptr;


	if (bWriteNumRepeat)
	{
		NumRepeats.Reserve(NumNodes);
		for (int i = 0; i < NumNodes; i++)
		{
			int32 NodeIdx = InCell->Nodes[i];
			NumRepeats.Add(NodeIdx, NumRepeats.FindOrAdd(NodeIdx, 0) + 1);
		}
	}

	if (bWriteCellHash) { InDataFacade->GetWritable<int64>(CellHashAttributeName, static_cast<int64>(InCell->GetCellHash()), true, PCGExData::EBufferInit::New); }
	if (bWriteArea) { InDataFacade->GetWritable<double>(AreaAttributeName, static_cast<double>(InCell->Data.Area), true, PCGExData::EBufferInit::New); }
	if (bWriteCompactness) { InDataFacade->GetWritable<double>(CompactnessAttributeName, static_cast<double>(InCell->Data.Compactness), true, PCGExData::EBufferInit::New); }
	if (TerminalBuffer) { for (int i = 0; i < NumNodes; i++) { TerminalBuffer->SetValue(i, InCluster->GetNode(InCell->Nodes[i])->IsLeaf()); } }
	if (RepeatBuffer) { for (int i = 0; i < NumNodes; i++) { RepeatBuffer->SetValue(i, NumRepeats[InCell->Nodes[i]] - 1); } }

	const TSharedPtr<PCGExData::TBuffer<int32>> VtxIDBuffer = bWriteVtxId ? InDataFacade->GetWritable(VtxIdAttributeName, 0, true, PCGExData::EBufferInit::New) : nullptr;
	if (VtxIDBuffer)
	{
		TSharedPtr<PCGExData::FPointIO> VtxIO = InCluster->VtxIO.Pin();
		const FPCGMetadataAttribute<int64>* VtxIDAttr = VtxIO ? VtxIO->FindConstAttribute<int64>(PCGExGraph::Attr_PCGExVtxIdx) : nullptr;

		if (VtxIO && VtxIDAttr)
		{
			TConstPCGValueRange<int64> MetadataEntries = VtxIO->GetIn()->GetConstMetadataEntryValueRange();
			for (int i = 0; i < NumNodes; i++)
			{
				const int32 PointIndex = InCluster->GetNodePointIndex(InCell->Nodes[i]);
				VtxIDBuffer->SetValue(i, PCGEx::H64A(VtxIDAttr->GetValueFromItemKey(MetadataEntries[PointIndex])));
			}
		}
	}
}
