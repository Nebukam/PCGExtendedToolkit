// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExBridgeEdgeClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExBridgeEdgeClusters"
#define PCGEX_NAMESPACE BridgeEdgeClusters

UPCGExBridgeEdgeClustersSettings::UPCGExBridgeEdgeClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExBridgeEdgeClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

void FPCGExBridgeEdgeClustersContext::BumpEdgeNum(const FPCGPoint& A, const FPCGPoint& B)
{
	FWriteScopeLock WriteScopeLock(NumEdgeLock);

	FPCGMetadataAttribute<int32>* EdgeNumAtt = static_cast<FPCGMetadataAttribute<int32>*>(CurrentIO->GetOut()->Metadata->GetMutableAttribute(PCGExGraph::Tag_EdgesNum));

	EdgeNumAtt->SetValue(A.MetadataEntry, EdgeNumAtt->GetValueFromItemKey(A.MetadataEntry) + 1);
	EdgeNumAtt->SetValue(B.MetadataEntry, EdgeNumAtt->GetValueFromItemKey(B.MetadataEntry) + 1);
}

PCGEX_INITIALIZE_ELEMENT(BridgeEdgeClusters)

PCGExData::EInit UPCGExBridgeEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBridgeEdgeClustersContext::~FPCGExBridgeEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	ConsolidatedEdges = nullptr;

	PCGEX_DELETE(Merger)
	PCGEX_DELETE(GraphBuilder)

	PCGEX_DELETE_TARRAY(Clusters)


	BridgedEdges.Empty();
	BridgedClusters.Empty();
}


bool FPCGExBridgeEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BridgeEdgeClusters)

	PCGEX_FWD(BridgeMethod)

	return true;
}

bool FPCGExBridgeEdgeClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBridgeEdgeClustersElement::Execute);

	PCGEX_CONTEXT(BridgeEdgeClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE_TARRAY(Context->Clusters)

		PCGEX_DELETE(Context->Merger)
		PCGEX_DELETE(Context->GraphBuilder)

		Context->BridgedEdges.Empty();
		Context->BridgedClusters.Empty();

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}
			
			if (Context->TaggedEdges->Entries.Num() == 1)
			{
				// No clusters to consolidate, just dump existing points
				Context->TaggedEdges->Entries[0]->InitializeOutput(PCGExData::EInit::DuplicateInput);
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
			}
			else
			{
				Context->ConsolidatedEdges = &Context->MainEdges->Emplace_GetRef(PCGExData::EInit::NewOutput);
				Context->ConsolidatedEdges->Tags->Reset(*Context->TaggedEdges->Entries[0]->Tags);

				// Merge all edges into a huge blurb
				for (PCGExData::FPointIO* EdgeIO : Context->TaggedEdges->Entries)
				{
					PCGExCluster::FCluster* NewCluster = new PCGExCluster::FCluster();
					Context->Clusters.Add(NewCluster);

					EdgeIO->CreateInKeys();
					Context->GetAsyncManager()->Start<FPCGExBuildCluster>(
						-1, Context->CurrentIO,
						NewCluster, EdgeIO, &Context->NodeIndicesMap, &Context->EdgeNumReader->Values);
				}

				Context->SetAsyncState(PCGExGraph::State_BuildingClusters);
			}
		}
	}

	if (Context->IsState(PCGExGraph::State_BuildingClusters))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		PCGExData::FPointIO* Head = nullptr;
		Context->BridgedEdges.Reset(Context->Clusters.Num());
		Context->BridgedClusters.Reset(Context->Clusters.Num());

		//Fetch all valid edges
		for (int i = 0; i < Context->Clusters.Num(); i++)
		{
			PCGExCluster::FCluster* Cluster = Context->Clusters[i];

			if (!Cluster->bValid)
			{
				Context->TaggedEdges->Entries[i]->Cleanup();
				continue;
			}

			Head = Context->TaggedEdges->Entries[i];
			Context->BridgedEdges.Add(Head);
			Context->BridgedClusters.Add(Cluster);
		}

		if (!Head)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some vtx/edges groups have no valid clusters."));
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
			return false;
		}

		if (Context->BridgedEdges.Num() != Context->TaggedEdges->Entries.Num())
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some vtx/edges groups have invalid clusters. Make sure to sanitize the input first."));
		}

		Context->ConsolidatedEdges = &Context->MainEdges->Emplace_GetRef(*Head, PCGExData::EInit::NewOutput);

		// Merge all valid edges into a single blurb
		Context->Merger = new FPCGExPointIOMerger(*Context->ConsolidatedEdges);
		Context->Merger->Append(Context->BridgedEdges);
		Context->Merger->Merge(Context->GetAsyncManager());

		Context->TotalPoints = Context->Merger->TotalPoints;

		Context->SetAsyncState(PCGExData::State_MergingData);
	}

	if (Context->IsState(PCGExData::State_MergingData))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		Context->Merger->Write();
		PCGEX_DELETE(Context->Merger);

		Context->SetState(PCGExGraph::State_ProcessingEdges);
	}

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		const int32 NumBounds = Context->TaggedEdges->Entries.Num();
		EPCGExBridgeClusterMethod SafeMethod = Context->BridgeMethod;

		if (NumBounds <= 4)
		{
			if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay) { SafeMethod = EPCGExBridgeClusterMethod::MostEdges; }
		}

		TArray<FBox> Bounds;
		Bounds.SetNumUninitialized(NumBounds);
		for (int i = 0; i < NumBounds; i++) { Bounds[i] = Context->BridgedEdges[i]->GetIn()->GetBounds(); }

		TArray<PCGExGraph::FUnsignedEdge*> Bridges;

		if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay)
		{
			PCGExGeo::TDelaunayTriangulation3* Delaunay = new PCGExGeo::TDelaunayTriangulation3();

			TArray<FPCGPoint> Points;
			Points.SetNum(NumBounds);
			for (int i = 0; i < NumBounds; i++) { Points[i].Transform.SetLocation(Bounds[i].GetCenter()); }

			if (Delaunay->PrepareFrom(Points))
			{
				TArray<PCGExGraph::FUnsignedEdge> DelaunayEdges;
				Delaunay->Generate();
				Delaunay->GetUniqueEdges(DelaunayEdges);

				for (const PCGExGraph::FUnsignedEdge& E : DelaunayEdges) { Bridges.Add(new PCGExGraph::FUnsignedEdge(E.Start, E.End)); }
			}

			PCGEX_DELETE(Delaunay)
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::LeastEdges)
		{
			TSet<int32> VisitedEdges;
			for (int i = 0; i < NumBounds; i++)
			{
				VisitedEdges.Add(i); // As to not connect to self or already connected
				double Distance = TNumericLimits<double>::Max();
				int32 ClosestIndex = -1;

				for (int j = 0; j < NumBounds; j++)
				{
					if (i == j || VisitedEdges.Contains(j)) { continue; }

					const double Dist = FVector::DistSquared(Bounds[i].GetCenter(), Bounds[j].GetCenter());
					if (Dist < Distance)
					{
						ClosestIndex = j;
						Distance = Dist;
					}
				}

				if (ClosestIndex == -1) { continue; }
				Bridges.Add(new PCGExGraph::FUnsignedEdge(i, ClosestIndex));
			}
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::MostEdges)
		{
			TSet<uint64> UniqueBridges;
			for (int i = 0; i < NumBounds; i++)
			{
				for (int j = 0; j < NumBounds; j++)
				{
					uint64 Hash = PCGExGraph::GetUnsignedHash64(i, j);
					if (i == j || UniqueBridges.Contains(Hash)) { continue; }
					UniqueBridges.Add(Hash);
					Bridges.Add(new PCGExGraph::FUnsignedEdge(i, j));
				}
			}
		}

		// Append new edges

		TArray<FPCGPoint>& MutableEdges = Context->ConsolidatedEdges->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = Context->ConsolidatedEdges->GetOut()->Metadata;

		for (const PCGExGraph::FUnsignedEdge* Bridge : Bridges)
		{
			FPCGPoint& EdgePoint = MutableEdges.Emplace_GetRef();
			Metadata->InitializeOnSet(EdgePoint.MetadataEntry);

			Context->GetAsyncManager()->Start<FPCGExCreateBridgeTask>(
				MutableEdges.Num() - 1, Context->ConsolidatedEdges,
				Context->Clusters[Bridge->Start],
				Context->Clusters[Bridge->End]);
		}

		PCGEX_DELETE_TARRAY(Bridges)

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
	}

	return Context->IsDone();
}

bool FPCGExCreateBridgeTask::ExecuteTask()
{
	FPCGExBridgeEdgeClustersContext* Context = Manager->GetContext<FPCGExBridgeEdgeClustersContext>();

	int32 IndexA = -1;
	int32 IndexB = -1;

	double Distance = TNumericLimits<double>::Max();

	//Brute force find closest points
	for (const PCGExCluster::FNode& Node : ClusterA->Nodes)
	{
		const PCGExCluster::FNode& OtherNode = ClusterB->Nodes[ClusterB->FindClosestNode(Node.Position)];

		if (const double Dist = FVector::DistSquared(Node.Position, OtherNode.Position);
			Dist < Distance)
		{
			IndexA = Node.PointIndex;
			IndexB = OtherNode.PointIndex;
			Distance = Dist;
		}
	}

	UPCGMetadata* EdgeMetadata = Context->ConsolidatedEdges->GetOut()->Metadata;
	UPCGMetadata* PointMetadata = Context->CurrentIO->GetOut()->Metadata;

	FPCGMetadataAttribute<int32>* StartIndexAtt = static_cast<FPCGMetadataAttribute<int32>*>(EdgeMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeStart));
	FPCGMetadataAttribute<int32>* EndIndexAtt = static_cast<FPCGMetadataAttribute<int32>*>(EdgeMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeEnd));
	FPCGMetadataAttribute<int32>* EdgeIndexAtt = static_cast<FPCGMetadataAttribute<int32>*>(PointMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeIndex));

	FPCGPoint& EdgePoint = PointIO->GetOut()->GetMutablePoints()[TaskIndex];

	const FPCGPoint& StartPoint = Context->CurrentIO->GetOutPoint(IndexA);
	const FPCGPoint& EndPoint = Context->CurrentIO->GetOutPoint(IndexB);

	EdgePoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));

	Context->BumpEdgeNum(StartPoint, EndPoint);

	StartIndexAtt->SetValue(EdgePoint.MetadataEntry, EdgeIndexAtt->GetValueFromItemKey(StartPoint.MetadataEntry));
	EndIndexAtt->SetValue(EdgePoint.MetadataEntry, EdgeIndexAtt->GetValueFromItemKey(EndPoint.MetadataEntry));

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
