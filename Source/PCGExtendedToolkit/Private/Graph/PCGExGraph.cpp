// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExGraph.h"

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

	bool FNetworkNode::GetNeighbors(const TArray<FIndexedEdge>& InEdges, TArray<int32>& OutIndices)
	{
		if (Edges.IsEmpty()) { return false; }
		for (const int32 Edge : Edges) { OutIndices.Add(InEdges[Edge].Other(Index)); }
		return true;
	}

	void FNetworkNode::Add(const FIndexedEdge& Edge)
	{
		Edges.AddUnique(Edge.Index);
	}

	void FEdgeCluster::Add(const FIndexedEdge& Edge, FEdgeNetwork* InNetwork)
	{
		FNetworkNode& NodeA = InNetwork->Nodes[Edge.Start];
		FNetworkNode& NodeB = InNetwork->Nodes[Edge.End];

		NodeA.Cluster = Id;
		NodeB.Cluster = Id;

		Nodes.Add(Edge.Start);
		Nodes.Add(Edge.End);

		Edges.Add(Edge.Index);
	}

	void FEdgeCluster::Append(const FEdgeCluster* Other, FEdgeNetwork* InNetwork)
	{
		for (const int32 Node : Other->Nodes) { InNetwork->Nodes[Node].Cluster = Id; }
		Nodes.Append(Other->Nodes);
		Edges.Append(Other->Edges);
	}

	void FEdgeCluster::Consolidate(const FEdgeNetwork* InNetwork)
	{
		TArray<int32> InvalidIndices;

		InvalidIndices.Reserve(Nodes.Num());
		for (const int32 NodeIndex : Nodes) { if (InNetwork->Nodes[NodeIndex].Cluster == -1) { InvalidIndices.Add(NodeIndex); } }

		if (InvalidIndices.Num() == Nodes.Num())
		{
			Nodes.Empty();
			Edges.Empty();
			return;
		}

		for (const int32 NodeIndex : InvalidIndices) { Nodes.Remove(NodeIndex); }

		InvalidIndices.Reset(Edges.Num());
		for (const int32 EdgeIndex : Edges)
		{
			const FIndexedEdge& Edge = InNetwork->Edges[EdgeIndex];
			if (!Edge.bValid ||
				!Nodes.Contains(Edge.Start) ||
				!Nodes.Contains(Edge.End))
			{
				InvalidIndices.Add(EdgeIndex);
			}
		}

		if (InvalidIndices.Num() == Edges.Num())
		{
			Nodes.Empty();
			Edges.Empty();
			return;
		}

		for (const int32 EdgeIndex : InvalidIndices) { Edges.Remove(EdgeIndex); }
		InvalidIndices.Empty();
	}

	bool FEdgeNetwork::InsertEdge(const int32 A, const int32 B)
	{
		const uint64 Hash = GetUnsignedHash64(A, B);

		{
			FReadScopeLock ReadLock(NetworkLock);
			if (UniqueEdges.Contains(Hash)) { return false; }
		}

		FWriteScopeLock WriteLock(NetworkLock);

		UniqueEdges.Add(Hash);

		const FIndexedEdge& Edge = Edges.Emplace_GetRef(Edges.Num(), A, B);

		FEdgeCluster** ClusterA = Clusters.Find(Nodes[A].Cluster);
		FEdgeCluster** ClusterB = Clusters.Find(Nodes[B].Cluster);

		if (!ClusterA && !ClusterB)
		{
			// New cluster
			FEdgeCluster* NewCluster = new FEdgeCluster();
			Clusters.Add(ClusterId, NewCluster);

			NewCluster->Id = ClusterId++;
			NewCluster->Add(Edge, this);
		}
		else if (ClusterA && ClusterB)
		{
			if (ClusterA != ClusterB)
			{
				// Merge clusters
				(*ClusterA)->Append(*ClusterB, this);
				Clusters.Remove((*ClusterB)->Id);
				PCGEX_DELETE(*ClusterB);
			}

			(*ClusterA)->Add(Edge, this);
		}
		else
		{
			// Expand cluster
			(ClusterA ? *ClusterA : *ClusterB)->Add(Edge, this);
		}

		return true;
	}

	void FEdgeCrossingsHandler::Prepare(const TArray<FPCGPoint>& InPoints)
	{
		for (int i = 0; i < NumEdges; i++)
		{
			const FUnsignedEdge& Edge = EdgeNetwork->Edges[i];
			FBox& NewBox = SegmentBounds.Emplace_GetRef(ForceInit);
			NewBox += InPoints[Edge.Start].Transform.GetLocation();
			NewBox += InPoints[Edge.End].Transform.GetLocation();
		}
	}

	void FEdgeCrossingsHandler::ProcessEdge(const int32 EdgeIndex, const TArray<FPCGPoint>& InPoints)

	{
		TArray<FIndexedEdge>& Edges = EdgeNetwork->Edges;

		const FUnsignedEdge& Edge = Edges[EdgeIndex];
		const FBox CurrentBox = SegmentBounds[EdgeIndex].ExpandBy(Tolerance);
		const FVector A1 = InPoints[Edge.Start].Transform.GetLocation();
		const FVector B1 = InPoints[Edge.End].Transform.GetLocation();

		for (int i = 0; i < NumEdges; i++)
		{
			if (CurrentBox.Intersect(SegmentBounds[i]))
			{
				const FUnsignedEdge& OtherEdge = Edges[i];
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
		TArray<FNetworkNode>& Nodes = EdgeNetwork->Nodes;
		TArray<FIndexedEdge>& Edges = EdgeNetwork->Edges;

		Nodes.Reserve(Nodes.Num() + Crossings.Num());

		for (const FEdgeCrossing& EdgeCrossing : Crossings)
		{
			Edges[EdgeCrossing.EdgeA].bValid = false;
			Edges[EdgeCrossing.EdgeB].bValid = false;

			FNetworkNode& NewNode = Nodes.Emplace_GetRef();
			NewNode.Index = Nodes.Num() - 1;
			NewNode.Edges.Reserve(4);
			NewNode.bCrossing = true;

			const FIndexedEdge& EdgeA = Edges[EdgeCrossing.EdgeA];
			const FIndexedEdge& EdgeB = Edges[EdgeCrossing.EdgeB];

			EdgeNetwork->InsertEdge(NewNode.Index, EdgeA.Start);
			EdgeNetwork->InsertEdge(NewNode.Index, EdgeA.End);
			EdgeNetwork->InsertEdge(NewNode.Index, EdgeB.Start);
			EdgeNetwork->InsertEdge(NewNode.Index, EdgeB.End);
		}
	}
}

bool FWriteClusterTask::ExecuteTask()
{
	PCGExGraph::FEdgeCluster* Cluster = *EdgeNetwork->Clusters.Find(TaskIndex);
	Cluster->Consolidate(EdgeNetwork);

	if (Cluster->Edges.IsEmpty() ||
		Cluster->Nodes.IsEmpty() ||
		Cluster->Edges.Num() < Min ||
		Cluster->Edges.Num() > Max) { return false; }

	TArray<FPCGPoint>& MutablePoints = ClusterIO->GetOut()->GetMutablePoints();
	MutablePoints.SetNum(Cluster->Edges.Num());

	ClusterIO->CreateOutKeys();

	PCGEx::TFAttributeWriter<int32>* EdgeStart = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeStartAttributeName, -1, false);
	PCGEx::TFAttributeWriter<int32>* EdgeEnd = new PCGEx::TFAttributeWriter<int32>(PCGExGraph::EdgeEndAttributeName, -1, false);

	EdgeStart->BindAndGet(*ClusterIO);
	EdgeEnd->BindAndGet(*ClusterIO);

	int32 PointIndex = 0;

	const TArray<FPCGPoint> Vertices = PointIO->GetOut()->GetPoints();

	auto WriteEdge = [&](const int32 Start, const int32 End)
	{
		MutablePoints[PointIndex].Transform.SetLocation(
			FMath::Lerp(
				Vertices[(EdgeStart->Values[PointIndex] = Start)].Transform.GetLocation(),
				Vertices[(EdgeEnd->Values[PointIndex] = End)].Transform.GetLocation(), 0.5));
		PointIndex++;
	};

	if (IndexRemap)
	{
		for (const int32 EdgeIndex : Cluster->Edges)
		{
			const PCGExGraph::FIndexedEdge& Edge = EdgeNetwork->Edges[EdgeIndex];
			WriteEdge(*IndexRemap->Find(Edge.Start), *IndexRemap->Find(Edge.End));
		}
	}
	else
	{
		for (const int32 EdgeIndex : Cluster->Edges)
		{
			const PCGExGraph::FUnsignedEdge& Edge = EdgeNetwork->Edges[EdgeIndex];
			WriteEdge(Edge.Start, Edge.End);
		}
	}

	EdgeStart->Write();
	EdgeEnd->Write();

	PCGEX_DELETE(EdgeStart)
	PCGEX_DELETE(EdgeEnd)

	return true;
}
