// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExCluster.h"

namespace PCGExGraph
{
	FSocket::~FSocket()
	{
		MatchingSockets.Empty();
		Cleanup();
	}

	void FSocket::Cleanup()
	{
		PCGEX_DELETE(TargetIndexWriter)
		PCGEX_DELETE(EdgeTypeWriter)
		PCGEX_DELETE(TargetIndexReader)
		PCGEX_DELETE(EdgeTypeReader)
	}

	void FSocket::DeleteFrom(const UPCGPointData* PointData) const
	{
		const FName NAME_Index = GetSocketPropertyName(SocketPropertyNameIndex);
		const FName NAME_EdgeType = GetSocketPropertyName(SocketPropertyNameEdgeType);

		if (PointData->Metadata->HasAttribute(NAME_Index)) { PointData->Metadata->DeleteAttribute(NAME_Index); }
		if (PointData->Metadata->HasAttribute(NAME_EdgeType)) { PointData->Metadata->DeleteAttribute(NAME_EdgeType); }
	}

	void FSocket::Write(const bool bDoCleanup)
	{
		if (TargetIndexWriter) { TargetIndexWriter->Write(); }
		if (EdgeTypeWriter) { EdgeTypeWriter->Write(); }
		if (bDoCleanup) { Cleanup(); }
	}

	void FSocket::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool ReadOnly)
	{
		Cleanup();

		bReadOnly = ReadOnly;

		const FName NAME_Index = GetSocketPropertyName(SocketPropertyNameIndex);
		const FName NAME_EdgeType = GetSocketPropertyName(SocketPropertyNameEdgeType);

		PCGExData::FPointIO& MutablePointIO = const_cast<PCGExData::FPointIO&>(PointIO);

		if (bReadOnly)
		{
			TargetIndexReader = new PCGEx::TFAttributeReader<int32>(NAME_Index);
			EdgeTypeReader = new PCGEx::TFAttributeReader<int32>(NAME_EdgeType);
			TargetIndexReader->Bind(MutablePointIO);
			EdgeTypeReader->Bind(MutablePointIO);
		}
		else
		{
			TargetIndexWriter = new PCGEx::TFAttributeWriter<int32>(NAME_Index, -1, false);
			EdgeTypeWriter = new PCGEx::TFAttributeWriter<int32>(NAME_EdgeType, static_cast<int32>(EPCGExEdgeType::Unknown), false);
			TargetIndexWriter->BindAndGet(MutablePointIO);
			EdgeTypeWriter->BindAndGet(MutablePointIO);
		}

		Descriptor.Bounds.LoadCurve();
	}

	void FSocket::SetData(const PCGMetadataEntryKey MetadataEntry, const FSocketMetadata& SocketMetadata) const
	{
		SetTargetIndex(MetadataEntry, SocketMetadata.Index);
		SetEdgeType(MetadataEntry, SocketMetadata.EdgeType);
	}

	void FSocket::SetTargetIndex(const int32 PointIndex, const int32 InValue) const
	{
		check(!bReadOnly)
		(*TargetIndexWriter)[PointIndex] = InValue;
	}

	int32 FSocket::GetTargetIndex(const int32 PointIndex) const
	{
		if (bReadOnly) { return (*TargetIndexReader)[PointIndex]; }
		return (*TargetIndexWriter)[PointIndex];
	}

	void FSocket::SetEdgeType(const int32 PointIndex, EPCGExEdgeType InEdgeType) const
	{
		check(!bReadOnly)
		(*EdgeTypeWriter)[PointIndex] = static_cast<int32>(InEdgeType);
	}

	EPCGExEdgeType FSocket::GetEdgeType(const int32 PointIndex) const
	{
		if (bReadOnly) { return static_cast<EPCGExEdgeType>((*EdgeTypeReader)[PointIndex]); }
		return static_cast<EPCGExEdgeType>((*EdgeTypeWriter)[PointIndex]);
	}

	FSocketMetadata FSocket::GetData(const int32 PointIndex) const
	{
		return FSocketMetadata(GetTargetIndex(PointIndex), GetEdgeType(PointIndex));
	}

	FName FSocket::GetSocketPropertyName(const FName PropertyName) const
	{
		const FString Separator = TEXT("/");
		return *(AttributeNameBase.ToString() + Separator + PropertyName.ToString());
	}

	void FSocketMapping::Initialize(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets)
	{
		Reset();
		Identifier = InIdentifier;
		for (FPCGExSocketDescriptor& Descriptor : InSockets)
		{
			if (!Descriptor.bEnabled) { continue; }

			Modifiers.Emplace_GetRef(Descriptor);
			LocalDirections.Emplace_GetRef(Descriptor);

			FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
			NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
			NewSocket.SocketIndex = NumSockets++;
			NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);
		}

		PostProcessSockets();
	}

	void FSocketMapping::InitializeWithOverrides(const FName InIdentifier, TArray<FPCGExSocketDescriptor>& InSockets, const FPCGExSocketGlobalOverrides& Overrides)
	{
		Reset();
		Identifier = InIdentifier;
		const FString PCGExName = TEXT("PCGEx");
		for (FPCGExSocketDescriptor& Descriptor : InSockets)
		{
			if (!Descriptor.bEnabled) { continue; }

			FProbeDistanceModifier& NewModifier = Modifiers.Emplace_GetRef(Descriptor);
			NewModifier.bEnabled = Overrides.bOverrideAttributeModifier ? Overrides.bApplyAttributeModifier : Descriptor.bApplyAttributeModifier;
			NewModifier.Descriptor = static_cast<FPCGExInputDescriptor>(Overrides.bOverrideAttributeModifier ? Overrides.AttributeModifier : Descriptor.AttributeModifier);

			FLocalDirection& NewLocalDirection = LocalDirections.Emplace_GetRef(Descriptor);
			NewLocalDirection.bEnabled = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.bDirectionVectorFromAttribute : Descriptor.bDirectionVectorFromAttribute;
			NewLocalDirection.Descriptor = Overrides.bOverrideDirectionVectorFromAttribute ? Overrides.AttributeDirectionVector : Descriptor.AttributeDirectionVector;

			FSocket& NewSocket = Sockets.Emplace_GetRef(Descriptor);
			NewSocket.AttributeNameBase = GetCompoundName(Descriptor.SocketName);
			NewSocket.SocketIndex = NumSockets++;
			NameToIndexMap.Add(NewSocket.GetName(), NewSocket.SocketIndex);

			if (Overrides.bOverrideRelativeOrientation) { NewSocket.Descriptor.bRelativeOrientation = Overrides.bRelativeOrientation; }
			if (Overrides.bOverrideAngle) { NewSocket.Descriptor.Bounds.Angle = Overrides.Angle; }
			if (Overrides.bOverrideMaxDistance) { NewSocket.Descriptor.Bounds.MaxDistance = Overrides.MaxDistance; }
			if (Overrides.bOverrideExclusiveBehavior) { NewSocket.Descriptor.bExclusiveBehavior = Overrides.bExclusiveBehavior; }
			if (Overrides.bOverrideDotOverDistance) { NewSocket.Descriptor.Bounds.DotOverDistance = Overrides.DotOverDistance; }
			if (Overrides.bOverrideOffsetOrigin) { NewSocket.Descriptor.OffsetOrigin = Overrides.OffsetOrigin; }

			NewSocket.Descriptor.Bounds.DotThreshold = FMath::Cos(NewSocket.Descriptor.Bounds.Angle * (PI / 180.0));
		}

		PostProcessSockets();
	}

	FName FSocketMapping::GetCompoundName(const FName SecondaryIdentifier) const
	{
		const FString Separator = TEXT("/");
		return *(TEXT("PCGEx") + Separator + Identifier.ToString() + Separator + SecondaryIdentifier.ToString()); // PCGEx/ParamsIdentifier/SocketIdentifier
	}

	void FSocketMapping::PrepareForPointData(const PCGExData::FPointIO& PointIO, const bool bReadOnly)
	{
		for (int i = 0; i < Sockets.Num(); i++)
		{
			Sockets[i].PrepareForPointData(PointIO, bReadOnly);
			Modifiers[i].Bind(PointIO);
			LocalDirections[i].Bind(PointIO);
		}
	}

	void FSocketMapping::GetSocketsInfos(TArray<FSocketInfos>& OutInfos)
	{
		OutInfos.Empty(NumSockets);
		for (int i = 0; i < NumSockets; i++)
		{
			FSocketInfos& Infos = OutInfos.Emplace_GetRef();
			Infos.Socket = &(Sockets[i]);
			Infos.MaxDistanceGetter = &(Modifiers[i]);
			Infos.LocalDirectionGetter = &(LocalDirections[i]);
		}
	}

	void FSocketMapping::Cleanup()
	{
		for (FSocket& Socket : Sockets) { Socket.Cleanup(); }
		for (FProbeDistanceModifier& Modifier : Modifiers) { Modifier.Cleanup(); }
		for (FLocalDirection& Direction : LocalDirections) { Direction.Cleanup(); }
	}

	void FSocketMapping::Reset()
	{
		Sockets.Empty();
		Modifiers.Empty();
		LocalDirections.Empty();
	}

	void FSocketMapping::PostProcessSockets()
	{
		for (FSocket& Socket : Sockets)
		{
			for (const FName MatchingSocketName : Socket.Descriptor.MatchingSlots)
			{
				FName OtherSocketName = GetCompoundName(MatchingSocketName);
				if (const int32* Index = NameToIndexMap.Find(OtherSocketName))
				{
					Socket.MatchingSockets.Add(*Index);
					if (Socket.Descriptor.bMirrorMatchingSockets) { Sockets[*Index].MatchingSockets.Add(Socket.SocketIndex); }
				}
			}
		}
	}

	void FNode::FixAdjacentNodes(const TArray<FIndexedEdge>& InEdges)
	{
		AdjacentNodes.SetNum(Edges.Num());
		for (int i = 0; i < Edges.Num(); i++) { AdjacentNodes[i] = InEdges[Edges[i]].Other(Index); }
	}

	void FNode::Add(const int32 EdgeIndex) { Edges.AddUnique(EdgeIndex); }

	void FSubGraph::Add(const FIndexedEdge& Edge, FGraph* InGraph)
	{
		FNode& NodeA = InGraph->Nodes[Edge.Start];
		FNode& NodeB = InGraph->Nodes[Edge.End];

		NodeA.bValid = true;
		NodeB.bValid = true;

		Nodes.Add(Edge.Start);
		Nodes.Add(Edge.End);

		Edges.Add(Edge.Index);
	}

	void FSubGraph::Consolidate(FGraph* InGraph)
	{
		if (bConsolidated) { return; }
		bConsolidated = true;

		TArray<int32> InvalidIndices;
		InvalidIndices.Reset(Edges.Num());
		for (const int32 EdgeIndex : Edges)
		{
			const FIndexedEdge& Edge = InGraph->Edges[EdgeIndex];
			if (!Edge.bValid) { InvalidIndices.Add(EdgeIndex); }
		}

		if (InvalidIndices.Num() == Edges.Num())
		{
			for (const int32 NodeIndex : Nodes) { InGraph->Nodes[NodeIndex].bValid = false; }
			Nodes.Empty();
			Edges.Empty();
		}
		else
		{
			for (const int32 EdgeIndex : InvalidIndices) { Edges.Remove(EdgeIndex); }
		}

		InvalidIndices.Empty();
	}

	void FSubGraph::Invalidate(FGraph* InGraph)
	{
		for (const int32 EdgeIndex : Edges) { InGraph->Edges[EdgeIndex].bValid = false; }
		for (const int32 NodeIndex : Nodes) { InGraph->Nodes[NodeIndex].bValid = false; }
	}

	bool FGraph::InsertEdge(const int32 A, const int32 B)
	{
		const uint64 Hash = GetUnsignedHash64(A, B);

		{
			FReadScopeLock ReadLock(GraphLock);
			if (UniqueEdges.Contains(Hash)) { return false; }
		}

		FWriteScopeLock WriteLock(GraphLock);

		UniqueEdges.Add(Hash);

		const FIndexedEdge& Edge = Edges.Emplace_GetRef(Edges.Num(), A, B);

		Nodes[A].Add(Edge.Index);
		Nodes[B].Add(Edge.Index);

		return true;
	}

	void FGraph::InsertEdges(const TArray<FUnsignedEdge>& InEdges)
	{
		for (const FUnsignedEdge& E : InEdges)
		{
			const uint64 Hash = E.GetUnsignedHash();
			if (UniqueEdges.Contains(Hash)) { continue; }

			UniqueEdges.Add(Hash);

			const FIndexedEdge& Edge = Edges.Emplace_GetRef(Edges.Num(), E.Start, E.End);

			Nodes[E.Start].Add(Edge.Index);
			Nodes[E.End].Add(Edge.Index);
		}
	}

	void FGraph::BuildSubGraphs()
	{
		TSet<int32> VisitedNodes;
		VisitedNodes.Reserve(Nodes.Num());

		for (int i = 0; i < Nodes.Num(); i++)
		{
			int32 NodeIndex = Nodes[i].Index;
			if (VisitedNodes.Contains(NodeIndex)) { continue; }

			const FNode& StartNode = Nodes[i];
			if (Nodes[NodeIndex].Edges.IsEmpty())
			{
				VisitedNodes.Add(NodeIndex);
				continue;
			}

			FSubGraph* SubGraph = SubGraphs.Add_GetRef(new FSubGraph());

			TQueue<int32> Queue;
			Queue.Enqueue(NodeIndex);

			int32 NextIndex = -1;
			while (Queue.Dequeue(NextIndex))
			{
				if (VisitedNodes.Contains(NextIndex)) { continue; }
				VisitedNodes.Add(NextIndex);

				for (int32 E : Nodes[NextIndex].Edges)
				{
					const FIndexedEdge& Edge = Edges[E];
					if (!Edge.bValid) { continue; }

					int32 OtherIndex = Edge.Other(NextIndex);
					SubGraph->Add(Edge, this);
					if (!VisitedNodes.Contains(OtherIndex)) { Queue.Enqueue(OtherIndex); }
				}
			}
		}
	}

	void FGraph::ConsolidateIndices(const bool bPrune)
	{
		if (bPrune)
		{
			int32 SafeIndex = 0;
			for (FNode& Node : Nodes) { Node.PointIndex = !Node.bValid || Node.Edges.IsEmpty() ? -1 : SafeIndex++; }
		}
		else { for (FNode& Node : Nodes) { Node.PointIndex = Node.Index; } }
	}

	void FGraph::Consolidate(const bool bPrune, const int32 Min, const int32 Max)
	{
		TArray<FSubGraph*> BadSubGraphs;

		for (FSubGraph* Subgraph : SubGraphs)
		{
			if (bRequiresConsolidation) { Subgraph->Consolidate(this); }
			if (!FMath::IsWithin(Subgraph->Edges.Num(), Min, Max)) { BadSubGraphs.Add(Subgraph); }
		}

		for (FSubGraph* SubGraph : BadSubGraphs)
		{
			SubGraphs.Remove(SubGraph);
			SubGraph->Invalidate(this);
			delete SubGraph;
		}

		bRequiresConsolidation = false;

		ConsolidateIndices(bPrune);
	}

	void FEdgeCrossingsHandler::Prepare(const TArray<FPCGPoint>& InPoints)
	{
		for (int i = 0; i < NumEdges; i++)
		{
			const FIndexedEdge& Edge = Graph->Edges[i];
			FBox& NewBox = SegmentBounds.Emplace_GetRef(ForceInit);
			NewBox += InPoints[Edge.Start].Transform.GetLocation();
			NewBox += InPoints[Edge.End].Transform.GetLocation();
		}
	}

	void FEdgeCrossingsHandler::ProcessEdge(const int32 EdgeIndex, const TArray<FPCGPoint>& InPoints)

	{
		TArray<FIndexedEdge>& Edges = Graph->Edges;

		const FIndexedEdge& Edge = Edges[EdgeIndex];
		const FBox CurrentBox = SegmentBounds[EdgeIndex].ExpandBy(Tolerance);
		const FVector A1 = InPoints[Edge.Start].Transform.GetLocation();
		const FVector B1 = InPoints[Edge.End].Transform.GetLocation();

		for (int i = 0; i < NumEdges; i++)
		{
			if (CurrentBox.Intersect(SegmentBounds[i]))
			{
				const FIndexedEdge& OtherEdge = Edges[i];
				FVector A2 = InPoints[OtherEdge.Start].Transform.GetLocation();
				FVector B2 = InPoints[OtherEdge.End].Transform.GetLocation();
				FVector A3;
				FVector B3;
				FMath::SegmentDistToSegment(A1, B1, A2, B2, A3, B3);
				const bool bIsEnd = A1 == A3 || B1 == A3 || A2 == A3 || B2 == A3 || A1 == B3 || B1 == B3 || A2 == B3 || B2 == B3;
				if (!bIsEnd && FVector::DistSquared(A3, B3) < SquaredTolerance)
				{
					FWriteScopeLock WriteLock(CrossingLock);
					FEdgeCrossing& EdgeCrossing = Crossings.Emplace_GetRef();
					EdgeCrossing.EdgeA = EdgeIndex;
					EdgeCrossing.EdgeB = i;
					EdgeCrossing.Center = FMath::Lerp(A3, B3, 0.5);
				}
			}
		}
	}

	void FEdgeCrossingsHandler::InsertCrossings()
	{
		TArray<FNode>& Nodes = Graph->Nodes;
		TArray<FIndexedEdge>& Edges = Graph->Edges;

		Nodes.Reserve(Nodes.Num() + Crossings.Num());
		if (!Crossings.IsEmpty()) { Graph->bRequiresConsolidation = true; }

		for (const FEdgeCrossing& EdgeCrossing : Crossings)
		{
			Edges[EdgeCrossing.EdgeA].bValid = false;
			Edges[EdgeCrossing.EdgeB].bValid = false;

			FNode& NewNode = Nodes.Emplace_GetRef();
			NewNode.Index = Nodes.Num() - 1;
			NewNode.Edges.Reserve(4);
			NewNode.bCrossing = true;

			const FIndexedEdge& EdgeA = Edges[EdgeCrossing.EdgeA];
			const FIndexedEdge& EdgeB = Edges[EdgeCrossing.EdgeB];

			Graph->InsertEdge(NewNode.Index, EdgeA.Start);
			Graph->InsertEdge(NewNode.Index, EdgeA.End);
			Graph->InsertEdge(NewNode.Index, EdgeB.Start);
			Graph->InsertEdge(NewNode.Index, EdgeB.End);
		}
	}

	void FGraphBuilder::EnableCrossings(const double Tolerance) { EdgeCrossings = new FEdgeCrossingsHandler(Graph, Tolerance); }

	void FGraphBuilder::EnablePointsPruning() { bPrunePoints = true; }

	bool FGraphBuilder::Compile(FPCGExPointsProcessorContext* InContext, int32 Min, int32 Max) const
	{
		if (EdgeCrossings) { EdgeCrossings->InsertCrossings(); }

		Graph->BuildSubGraphs();

		if (bPrunePoints)
		{
			Graph->Consolidate(true, Min, Max);
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

			if (!MutablePoints.IsEmpty())
			{
				//Assume points were filled before, and remove them from the current array
				TArray<FPCGPoint> ValidPoints;
				ValidPoints.Reserve(MutablePoints.Num());

				for (FNode& Node : Graph->Nodes)
				{
					if (Node.bCrossing || !Node.bValid) { continue; }
					Node.PointIndex = ValidPoints.Add(MutablePoints[Node.Index]);
				}

				MutablePoints.Reset(ValidPoints.Num());
				MutablePoints.Append(ValidPoints);
			}
			else
			{
				const int32 NumMaxNodes = Graph->Nodes.Num();
				MutablePoints.Reserve(NumMaxNodes);

				for (FNode& Node : Graph->Nodes)
				{
					if (Node.bCrossing || !Node.bValid) { continue; }
					Node.PointIndex = MutablePoints.Add(PointIO->GetInPoint(Node.Index));
				}
			}

			if (EdgeCrossings)
			{
				for (int i = 0; i < EdgeCrossings->Crossings.Num(); i++)
				{
					if (!Graph->Nodes[EdgeCrossings->StartIndex + i].bValid) { continue; }
					MutablePoints.Last().Transform.SetLocation(EdgeCrossings->Crossings[i].Center);
				}
			}
		}
		else
		{
			Graph->ConsolidateIndices(false);

			if (EdgeCrossings)
			{
				TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
				for (const FEdgeCrossing& Crossing : EdgeCrossings->Crossings)
				{
					MutablePoints.Emplace_GetRef().Transform.SetLocation(Crossing.Center);
				}
			}
		}

		if (Graph->SubGraphs.IsEmpty())
		{
			PointIO->GetOut()->Metadata->DeleteAttribute(PUIDAttributeName); // Unmark
			return false;
		}


		for (FSubGraph* SubGraph : Graph->SubGraphs)
		{
			PCGExData::FPointIO& EdgeIO = EdgesIO->Emplace_GetRef(PCGExData::EInit::NewOutput);
			Markings->Add(EdgeIO);
			InContext->GetAsyncManager()->Start<FWriteSubGraphEdgesTask>(-1, PointIO, &EdgeIO, Graph, SubGraph, Min, Max);
		}

		return true;
	}

	void FGraphBuilder::Write(FPCGExPointsProcessorContext* InContext) const
	{
		Markings->UpdateMark();
		EdgesIO->OutputTo(InContext, true);
	}
}

bool FWriteSubGraphEdgesTask::ExecuteTask()
{
	if (Graph->bRequiresConsolidation) { SubGraph->Consolidate(Graph); }

	if (SubGraph->Edges.IsEmpty() ||
		SubGraph->Nodes.IsEmpty() ||
		!FMath::IsWithin(SubGraph->Edges.Num(), Min, Max))
	{
		return false;
	}

	TArray<FPCGPoint>& MutablePoints = EdgeIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(SubGraph->Edges.Num());

	EdgeIO->CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);

	EdgeStart->BindAndGet(*EdgeIO);
	EdgeEnd->BindAndGet(*EdgeIO);

	int32 PointIndex = 0;

	const TArray<FPCGPoint> Vertices = PointIO->GetOut()->GetPoints();

	for (const int32 EdgeIndex : SubGraph->Edges)
	{
		const PCGExGraph::FIndexedEdge& Edge = Graph->Edges[EdgeIndex];
		MutablePoints[PointIndex].Transform.SetLocation(
			FMath::Lerp(
				Vertices[(EdgeStart->Values[PointIndex] = Graph->Nodes[Edge.Start].PointIndex)].Transform.GetLocation(),
				Vertices[(EdgeEnd->Values[PointIndex] = Graph->Nodes[Edge.End].PointIndex)].Transform.GetLocation(), 0.5));
		PointIndex++;
	}

	EdgeStart->Write();
	EdgeEnd->Write();

	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)

	return true;
}
