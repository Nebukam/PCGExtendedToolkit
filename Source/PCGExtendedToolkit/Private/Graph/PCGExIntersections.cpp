// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExIntersections.h"

#include "IntVectorTypes.h"
#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	FVector FCompoundNode::UpdateCenter(const PCGExData::FIdxCompoundList* PointsCompounds, PCGExData::FPointIOCollection* IOGroup)
	{
		Center = FVector::ZeroVector;
		double Divider = 0;

		uint32 IOIndex = 0;
		uint32 PointIndex = 0;

		PCGExData::FIdxCompound* Compound = (*PointsCompounds)[Index];
		for (const uint64 FuseHash : Compound->CompoundedHashSet)
		{
			Divider++;
			PCGEx::H64(FuseHash, IOIndex, PointIndex);
			Center += IOGroup->Pairs[IOIndex]->GetInPoint(PointIndex).Transform.GetLocation();
		}

		Center /= Divider;
		return Center;
	}

	FCompoundNode* FCompoundGraph::InsertPoint(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		const FVector Origin = Point.Transform.GetLocation();
		FCompoundNode* Node;

		if (!bFusePoints)
		{
			FWriteScopeLock WriteLock(OctreeLock);
			Node = new FCompoundNode(Point, Origin, Nodes.Num());
			Nodes.Add(Node);
			PointsCompounds->New()->Add(IOIndex, PointIndex);
			return Node;
		}

		if (!Octree)
		{
			FWriteScopeLock WriteLock(OctreeLock);

			const uint32 GridKey = PCGEx::GH(Origin, CWTolerance);

			if (FCompoundNode** NodePtr = GridTree.Find(GridKey))
			{
				Node = *NodePtr;
				PointsCompounds->Add(Node->Index, IOIndex, PointIndex);
			}
			else
			{
				Node = new FCompoundNode(Point, Origin, Nodes.Num());
				Nodes.Add(Node);
				PointsCompounds->New()->Add(IOIndex, PointIndex);
				GridTree.Add(GridKey, Node);
			}

			return Node;
		}

		int32 Index = -1;

		if (FuseDetails.bComponentWiseTolerance)
		{
			FReadScopeLock ReadLock(OctreeLock);
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FuseDetails.Tolerances);
			Octree->FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						Index = ExistingNode->Index;
						return false;
					}
					return true;
				});
		}
		else
		{
			FReadScopeLock ReadLock(OctreeLock);
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FVector(FuseDetails.Tolerance));
			Octree->FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						Index = ExistingNode->Index;
						return false;
					}
					return true;
				});
		}

		{
			FWriteScopeLock WriteLock(OctreeLock);

			if (Index != -1)
			{
				PointsCompounds->Add(Index, IOIndex, PointIndex);
				return Nodes[Index];
			}

			Node = new FCompoundNode(Point, Origin, Nodes.Num());
			Nodes.Add(Node);
			Octree->AddElement(Node);
			PointsCompounds->New()->Add(IOIndex, PointIndex);
		}

		return Node;
	}

	FCompoundNode* FCompoundGraph::InsertPointUnsafe(const FPCGPoint& Point, const int32 IOIndex, const int32 PointIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundGraph::InsertPointUnsafe);

		const FVector Origin = Point.Transform.GetLocation();
		FCompoundNode* Node;

		if (!bFusePoints)
		{
			FCompoundNode* NewNode = new FCompoundNode(Point, Origin, Nodes.Num());
			Nodes.Add(NewNode);
			PointsCompounds->New()->Add(IOIndex, PointIndex);
			return NewNode;
		}

		if (!Octree)
		{
			const uint32 GridKey = PCGEx::GH(Origin, CWTolerance);

			if (FCompoundNode** NodePtr = GridTree.Find(GridKey))
			{
				Node = *NodePtr;
				PointsCompounds->Add(Node->Index, IOIndex, PointIndex);
			}
			else
			{
				Node = new FCompoundNode(Point, Origin, Nodes.Num());
				Nodes.Add(Node);
				PointsCompounds->New()->Add(IOIndex, PointIndex);
				GridTree.Add(GridKey, Node);
			}

			return Node;
		}

		int32 Index = -1;

		if (FuseDetails.bComponentWiseTolerance)
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FuseDetails.Tolerances);
			Octree->FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						Index = ExistingNode->Index;
						return false;
					}
					return true;
				});
		}
		else
		{
			const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Origin, FVector(FuseDetails.Tolerance));
			Octree->FindFirstElementWithBoundsTest(
				Box, [&](const FCompoundNode* ExistingNode)
				{
					if (FuseDetails.IsWithinToleranceComponentWise(Point, ExistingNode->Point))
					{
						Index = ExistingNode->Index;
						return false;
					}
					return true;
				});
		}

		if (Index != -1)
		{
			PointsCompounds->Add(Index, IOIndex, PointIndex);
			return Nodes[Index];
		}

		FCompoundNode* NewNode = new FCompoundNode(Point, Origin, Nodes.Num());
		Nodes.Add(NewNode);
		Octree->AddElement(NewNode);
		PointsCompounds->New()->Add(IOIndex, PointIndex);

		return NewNode;
	}

	PCGExData::FIdxCompound* FCompoundGraph::InsertEdge(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCompoundGraph::InsertEdge);

		FCompoundNode* StartVtx;
		FCompoundNode* EndVtx;

		{
			FWriteScopeLock WriteLockOctree(OctreeLock);

			StartVtx = InsertPointUnsafe(From, FromIOIndex, FromPointIndex);
			EndVtx = InsertPointUnsafe(To, ToIOIndex, ToPointIndex);


			if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

			StartVtx->Adjacency.Add(EndVtx->Index);
			EndVtx->Adjacency.Add(StartVtx->Index);
		}

		PCGExData::FIdxCompound* EdgeIdx = nullptr;

		if (EdgeIOIndex == -1) { return EdgeIdx; } // Skip edge management

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);

		{
			FReadScopeLock ReadLockEdges(EdgesLock);
			if (const FIndexedEdge* Edge = Edges.Find(H)) { EdgeIdx = EdgesCompounds->Compounds[Edge->EdgeIndex]; }
		}

		{
			FWriteScopeLock WriteLockEdges(EdgesLock);
			if (!EdgeIdx)
			{
				EdgeIdx = EdgesCompounds->New();
				Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
				EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
			}
			else
			{
				EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
			}
		}

		return EdgeIdx;
	}

	PCGExData::FIdxCompound* FCompoundGraph::InsertEdgeUnsafe(const FPCGPoint& From, const int32 FromIOIndex, const int32 FromPointIndex, const FPCGPoint& To, const int32 ToIOIndex, const int32 ToPointIndex, const int32 EdgeIOIndex, const int32 EdgePointIndex)
	{
		FCompoundNode* StartVtx = InsertPointUnsafe(From, FromIOIndex, FromPointIndex);
		FCompoundNode* EndVtx = InsertPointUnsafe(To, ToIOIndex, ToPointIndex);

		if (StartVtx == EndVtx) { return nullptr; } // Edge got fused entirely

		StartVtx->Adjacency.Add(EndVtx->Index);
		EndVtx->Adjacency.Add(StartVtx->Index);

		PCGExData::FIdxCompound* EdgeIdx = nullptr;
		if (EdgeIOIndex == -1) { return EdgeIdx; } // Skip edge management

		const uint64 H = PCGEx::H64U(StartVtx->Index, EndVtx->Index);
		if (const FIndexedEdge* Edge = Edges.Find(H))
		{
			EdgeIdx = EdgesCompounds->Compounds[Edge->EdgeIndex];
			EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
		}
		else
		{
			EdgeIdx = EdgesCompounds->New();
			Edges.Add(H, FIndexedEdge(Edges.Num(), StartVtx->Index, EndVtx->Index));
			EdgeIdx->Add(EdgeIOIndex, EdgePointIndex);
		}

		return EdgeIdx;
	}

	void FCompoundGraph::GetUniqueEdges(TArray<FUnsignedEdge>& OutEdges)
	{
		OutEdges.Empty(Nodes.Num() * 4);
		TSet<uint64> UniqueEdges;
		bool bIsAlreadyPresent;
		for (const FCompoundNode* Node : Nodes)
		{
			for (const int32 OtherNodeIndex : Node->Adjacency)
			{
				const uint64 Hash = PCGEx::H64U(Node->Index, OtherNodeIndex);
				UniqueEdges.Add(Hash, &bIsAlreadyPresent);

				if (bIsAlreadyPresent) { continue; }

				OutEdges.Emplace(Node->Index, OtherNodeIndex);
			}
		}
		UniqueEdges.Empty();
	}

	void FCompoundGraph::WriteMetadata(TMap<int32, FGraphNodeMetadata*>& OutMetadata)
	{
		for (const FCompoundNode* Node : Nodes)
		{
			FGraphNodeMetadata* NodeMeta = FGraphNodeMetadata::GetOrCreate(Node->Index, OutMetadata);
			NodeMeta->CompoundSize = Node->Adjacency.Num();
			NodeMeta->bCompounded = NodeMeta->CompoundSize > 1;
		}
	}

	FPointEdgeIntersections::FPointEdgeIntersections(
		FGraph* InGraph,
		FCompoundGraph* InCompoundGraph,
		PCGExData::FPointIO* InPointIO,
		const FPCGExPointEdgeIntersectionDetails* InDetails)
		: PointIO(InPointIO), Graph(InGraph), CompoundGraph(InCompoundGraph), Details(InDetails)
	{
		const TArray<FPCGPoint>& Points = InPointIO->GetOutIn()->GetPoints();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		for (const FIndexedEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.EdgeIndex].Init(
				Edge.EdgeIndex,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Details->FuseDetails.Tolerance);
		}
	}

	void FPointEdgeIntersections::FindIntersections(FPCGExPointsProcessorContext* InContext)
	{
		for (const FIndexedEdge& Edge : Graph->Edges)
		{
			if (!Edge.bValid) { continue; }
			InContext->GetAsyncManager()->Start<PCGExGraphTask::FFindPointEdgeIntersections>(Edge.EdgeIndex, PointIO, this);
		}
	}

	void FPointEdgeIntersections::Insert()
	{
		FIndexedEdge NewEdge = FIndexedEdge{};

		for (FPointEdgeProxy& PointEdgeProxy : Edges)
		{
			if (PointEdgeProxy.CollinearPoints.IsEmpty()) { continue; }

			FIndexedEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];
			SplitEdge.bValid = false; // Invalidate existing edge
			PointEdgeProxy.CollinearPoints.Sort([](const FPESplit& A, const FPESplit& B) { return A.Time < B.Time; });

			const int32 FirstIndex = SplitEdge.Start;
			const int32 LastIndex = SplitEdge.End;

			int32 NodeIndex = -1;

			int32 PrevIndex = FirstIndex;
			for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
			{
				NodeIndex = Split.NodeIndex;

				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: IOIndex required
				PrevIndex = NodeIndex;

				FGraphNodeMetadata* NodeMetadata = FGraphNodeMetadata::GetOrCreate(NodeIndex, Graph->NodeMetadata);
				NodeMetadata->Type = EPCGExIntersectionType::PointEdge;

				FGraphEdgeMetadata* EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(NewEdge.EdgeIndex, SplitEdge.EdgeIndex, Graph->EdgeMetadata);
				EdgeMetadata->Type = EPCGExIntersectionType::PointEdge;

				if (Details->bSnapOnEdge)
				{
					PointIO->GetMutablePoint(Graph->Nodes[Split.NodeIndex].PointIndex).Transform.SetLocation(Split.ClosestPoint);
				}
			}

			Graph->InsertEdge(NodeIndex, LastIndex, NewEdge, SplitEdge.IOIndex); // Insert last edge
		}
	}

	void FPointEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		const FPointEdgeProxy& PointEdgeProxy = Edges[Index];

		if (PointEdgeProxy.CollinearPoints.IsEmpty()) { return; }

		const FIndexedEdge& SplitEdge = Graph->Edges[PointEdgeProxy.EdgeIndex];

		const PCGExData::FPointRef A = PointIO->GetOutPointRef(SplitEdge.Start);
		const PCGExData::FPointRef B = PointIO->GetOutPointRef(SplitEdge.End);

		for (const FPESplit Split : PointEdgeProxy.CollinearPoints)
		{
			const PCGExData::FPointRef Target = PointIO->GetOutPointRef(Graph->Nodes[Split.NodeIndex].PointIndex);
			FPCGPoint& Pt = PointIO->GetMutablePoint(Target.Index);

			FVector PreBlendLocation = Pt.Transform.GetLocation();

			Blender->PrepareForBlending(Target);
			Blender->Blend(A, B, Target, 0.5);
			Blender->CompleteBlending(Target, 2, 1);

			Pt.Transform.SetLocation(PreBlendLocation);
		}
	}

	FEdgeEdgeIntersections::FEdgeEdgeIntersections(
		FGraph* InGraph,
		FCompoundGraph* InCompoundGraph,
		PCGExData::FPointIO* InPointIO,
		const FPCGExEdgeEdgeIntersectionDetails* InDetails)
		: PointIO(InPointIO), Graph(InGraph), CompoundGraph(InCompoundGraph), Details(InDetails)
	{
		const TArray<FPCGPoint>& Points = InPointIO->GetOutIn()->GetPoints();

		const int32 NumEdges = InGraph->Edges.Num();
		Edges.SetNum(NumEdges);

		Octree = TEdgeOctree(InCompoundGraph->Bounds.GetCenter(), InCompoundGraph->Bounds.GetExtent().Length() + (Details->Tolerance * 2));

		for (const FIndexedEdge& Edge : InGraph->Edges)
		{
			if (!Edge.bValid) { continue; }
			Edges[Edge.EdgeIndex].Init(
				Edge.EdgeIndex,
				Points[Edge.Start].Transform.GetLocation(),
				Points[Edge.End].Transform.GetLocation(),
				Details->Tolerance);

			Octree.AddElement(&Edges[Edge.EdgeIndex]);
		}
	}

	void FEdgeEdgeIntersections::FindIntersections(FPCGExPointsProcessorContext* InContext)
	{
		for (const FIndexedEdge& Edge : Graph->Edges)
		{
			if (!Edge.bValid) { continue; }
			InContext->GetAsyncManager()->Start<PCGExGraphTask::FFindEdgeEdgeIntersections>(Edge.EdgeIndex, PointIO, this);
		}
	}

	void FEdgeEdgeIntersections::Insert()
	{
		FIndexedEdge NewEdge = FIndexedEdge{};

		// Insert new nodes
		Graph->AddNodes(Crossings.Num()); //const TArrayView<FNode> NewNodes = 

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		MutablePoints.SetNum(Graph->Nodes.Num());

		// Now handled by intersection blending
		//for (int i = 0; i < NewNodes.Num(); i++) { MutablePoints[NewNodes[i].NodeIndex].Transform.SetLocation(Crossings[i]->Split.Center); }

		for (FEdgeEdgeProxy& EdgeProxy : Edges)
		{
			if (EdgeProxy.Intersections.IsEmpty()) { continue; }

			Graph->Edges[EdgeProxy.EdgeIndex].bValid = false; // Invalidate existing edge
			const FIndexedEdge SplitEdge = Graph->Edges[EdgeProxy.EdgeIndex];

			EdgeProxy.Intersections.Sort(
				[&](const FEECrossing& A, const FEECrossing& B)
				{
					return A.GetTime(EdgeProxy.EdgeIndex) > B.GetTime(EdgeProxy.EdgeIndex);
				});

			const int32 FirstIndex = SplitEdge.Start;
			const int32 LastIndex = SplitEdge.End;

			int32 NodeIndex = -1;

			int32 PrevIndex = FirstIndex;
			for (const FEECrossing* Crossing : EdgeProxy.Intersections)
			{
				NodeIndex = Crossing->NodeIndex;
				Graph->InsertEdge(PrevIndex, NodeIndex, NewEdge, SplitEdge.IOIndex); //TODO: this is the wrong edge IOIndex
				PrevIndex = NodeIndex;

				FGraphNodeMetadata* NodeMetadata = FGraphNodeMetadata::GetOrCreate(NodeIndex, Graph->NodeMetadata);
				NodeMetadata->Type = EPCGExIntersectionType::EdgeEdge;

				FGraphEdgeMetadata* EdgeMetadata = FGraphEdgeMetadata::GetOrCreate(NewEdge.EdgeIndex, EdgeProxy.EdgeIndex, Graph->EdgeMetadata);
				EdgeMetadata->Type = EPCGExIntersectionType::EdgeEdge;
			}

			Graph->InsertEdge(NodeIndex, LastIndex, NewEdge, SplitEdge.IOIndex); // Insert last edge
		}
	}

	void FEdgeEdgeIntersections::BlendIntersection(const int32 Index, PCGExDataBlending::FMetadataBlender* Blender) const
	{
		const FEECrossing* Crossing = Crossings[Index];

		const PCGExData::FPointRef Target = PointIO->GetOutPointRef(Graph->Nodes[Crossing->NodeIndex].PointIndex);
		Blender->PrepareForBlending(Target);

		const PCGExData::FPointRef A1 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeA].Start].PointIndex);
		const PCGExData::FPointRef A2 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeA].End].PointIndex);
		const PCGExData::FPointRef B1 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeB].Start].PointIndex);
		const PCGExData::FPointRef B2 = PointIO->GetOutPointRef(Graph->Nodes[Graph->Edges[Crossing->EdgeB].End].PointIndex);

		Blender->Blend(Target, A1, Target, Crossing->Split.TimeA);
		Blender->Blend(Target, A2, Target, 1 - Crossing->Split.TimeA);
		Blender->Blend(Target, B1, Target, Crossing->Split.TimeB);
		Blender->Blend(Target, B2, Target, 1 - Crossing->Split.TimeB);

		Blender->CompleteBlending(Target, 4, 2);

		PointIO->GetMutablePoint(Target.Index).Transform.SetLocation(Crossing->Split.Center);
	}
}

namespace PCGExGraphTask
{
	bool FFindPointEdgeIntersections::ExecuteTask()
	{
		FindCollinearNodes(IntersectionList, TaskIndex, PointIO->GetOutIn());
		return true;
	}

	bool FInsertPointEdgeIntersections::ExecuteTask()
	{
		IntersectionList->Insert();
		return true;
	}

	bool FFindEdgeEdgeIntersections::ExecuteTask()
	{
		FindOverlappingEdges(IntersectionList, TaskIndex);
		return true;
	}

	bool FInsertEdgeEdgeIntersections::ExecuteTask()
	{
		IntersectionList->Insert();
		return true;
	}
}
