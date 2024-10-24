// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExConnectClusters.h"
#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExConnectClusters"
#define PCGEX_NAMESPACE ConnectClusters

PCGExData::EInit UPCGExConnectClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }
PCGExData::EInit UPCGExConnectClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(ConnectClusters)

bool FPCGExConnectClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ConnectClusters)

	PCGEX_FWD(CarryOverDetails)
	Context->CarryOverDetails.Init();

	PCGEX_FWD(ProjectionDetails)
	PCGEX_FWD(GraphBuilderDetails)

	return true;
}

bool FPCGExConnectClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ConnectClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExBridgeClusters::FProcessorBatch>(
			[&](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries)
			{
				if (Entries->Entries.Num() == 1)
				{
					// No clusters to consolidate, just dump existing points
					Context->CurrentIO->InitializeOutput(Context, PCGExData::EInit::Forward);
					Entries->Entries[0]->InitializeOutput(Context, PCGExData::EInit::Forward);
					return false;
				}

				return true;
			},
			[&](const TSharedPtr<PCGExBridgeClusters::FProcessorBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			if (!Settings->bMuteNoBridgeWarning) { PCGE_LOG(Warning, GraphAndLog, FTEXT("No bridge was created.")); }
			Context->OutputPointsAndEdges();
			return true;
		}
	}


	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	for (const TSharedPtr<PCGExClusterMT::FClusterProcessorBatchBase>& Batch : Context->Batches)
	{
		const TSharedPtr<PCGExBridgeClusters::FProcessorBatch> BridgeBatch = StaticCastSharedPtr<PCGExBridgeClusters::FProcessorBatch>(Batch);
		const int32 ClusterId = BridgeBatch->VtxDataFacade->GetOut()->GetUniqueID();
		WriteMark(BridgeBatch->CompoundedEdgesDataFacade->Source, PCGExGraph::Tag_ClusterId, ClusterId);

		FString OutId;
		PCGExGraph::SetClusterVtx(BridgeBatch->VtxDataFacade->Source, OutId);
		PCGExGraph::MarkClusterEdges(BridgeBatch->CompoundedEdgesDataFacade->Source, OutId);
	}

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExBridgeClusters
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBridgeClusters::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		return true;
	}

	void FProcessor::ProcessSingleEdge(const int32 EdgeIndex, PCGExGraph::FIndexedEdge& Edge, const int32 LoopIdx, const int32 Count)
	{
	}

	void FProcessor::CompleteWork()
	{
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
		InVtx->InitializeOutput(InContext, PCGExData::EInit::DuplicateInput);
	}

	void FProcessorBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectClusters)

		const TSharedPtr<PCGExData::FPointIO> ConsolidatedEdges = Context->MainEdges->Emplace_GetRef(PCGExData::EInit::NewOutput);
		CompoundedEdgesDataFacade = MakeShared<PCGExData::FFacade>(ConsolidatedEdges.ToSharedRef());

		TBatch<FProcessor>::Process();

		// Start merging right away
		TSet<FName> IgnoreAttributes = {PCGExGraph::Tag_ClusterId};

		Merger = MakeShared<FPCGExPointIOMerger>(CompoundedEdgesDataFacade.ToSharedRef());
		Merger->Append(Edges);
		Merger->Merge(AsyncManager, &Context->CarryOverDetails);
	}

	bool FProcessorBatch::PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor)
	{
		CompoundedEdgesDataFacade->Source->Tags->Append(ClusterProcessor->EdgeDataFacade->Source->Tags.ToSharedRef());
		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectClusters)

		const int32 NumValidClusters = GatherValidClusters();

		if (Processors.Num() != NumValidClusters)
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some vtx/edges groups have invalid clusters. Make sure to sanitize the input first."));
		}

		if (ValidClusters.IsEmpty()) { return; } // Skip work completion entirely

		CompoundedEdgesDataFacade->Write(AsyncManager); // Write base attributes value while finding bridges

		const int32 NumBounds = ValidClusters.Num();
		EPCGExBridgeClusterMethod SafeMethod = Settings->BridgeMethod;

		if (NumBounds <= 4)
		{
			if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay3D) { SafeMethod = EPCGExBridgeClusterMethod::MostEdges; }
		}
		else if (NumBounds <= 3)
		{
			if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay2D) { SafeMethod = EPCGExBridgeClusterMethod::MostEdges; }
		}

		// First find which cluster are connected

		TArray<FBox> Bounds;
		PCGEx::InitArray(Bounds, NumBounds);
		for (int i = 0; i < NumBounds; i++) { Bounds[i] = ValidClusters[i]->Bounds; }

		if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay3D)
		{
			const TUniquePtr<PCGExGeo::TDelaunay3> Delaunay = MakeUnique<PCGExGeo::TDelaunay3>();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; i++) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process<false, false>(Positions)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Delaunay 3D failed. Are points coplanar? If so, use Delaunay 2D instead.")); }

			Positions.Empty();
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay2D)
		{
			const TUniquePtr<PCGExGeo::TDelaunay2> Delaunay = MakeUnique<PCGExGeo::TDelaunay2>();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; i++) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, Context->ProjectionDetails)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Delaunay 2D failed.")); }

			Positions.Empty();
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::LeastEdges)
		{
			TSet<int32> VisitedEdges;
			for (int i = 0; i < NumBounds; i++)
			{
				VisitedEdges.Add(i); // As to not connect to self or already connected
				double Distance = MAX_dbl;
				int32 ClosestIndex = -1;

				for (int j = 0; j < NumBounds; j++)
				{
					if (i == j || VisitedEdges.Contains(j)) { continue; }

					if (const double Dist = FVector::DistSquared(Bounds[i].GetCenter(), Bounds[j].GetCenter());
						Dist < Distance)
					{
						ClosestIndex = j;
						Distance = Dist;
					}
				}

				if (ClosestIndex == -1) { continue; }

				Bridges.Add(PCGEx::H64(i, ClosestIndex));
			}
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::MostEdges)
		{
			for (int i = 0; i < NumBounds; i++)
			{
				for (int j = 0; j < NumBounds; j++)
				{
					if (i == j) { continue; }
					Bridges.Add(PCGEx::H64U(i, j));
				}
			}
		}
	}

	void FProcessorBatch::Write()
	{
		const TSharedRef<PCGExData::FPointIO> ConsolidatedEdges = CompoundedEdgesDataFacade->Source;

		for (const uint64 Bridge : Bridges)
		{
			int32 EdgePointIndex;
			ConsolidatedEdges->NewPoint(EdgePointIndex);

			uint32 Start;
			uint32 End;
			PCGEx::H64(Bridge, Start, End);

			AsyncManager->Start<FPCGExCreateBridgeTask>(EdgePointIndex, ConsolidatedEdges, SharedThis(this), ValidClusters[Start], ValidClusters[End]);
		}

		// Force writing cluster ID to Vtx, otherwise we inherit from previous metadata.
		const uint32 ClusterId = VtxDataFacade->GetOut()->GetUniqueID();
		const TSharedPtr<PCGExData::TBuffer<int32>> ClusterIdBuffer = CompoundedEdgesDataFacade->GetWritable<int32>(PCGExGraph::Tag_ClusterId, true);
		for (TArray<int32>& OutValues = *ClusterIdBuffer->GetOutValues(); int32& Id : OutValues) { Id = ClusterId; }
		PCGExMT::Write(AsyncManager, ClusterIdBuffer);
	}


	bool FPCGExCreateBridgeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		int32 IndexA = -1;
		int32 IndexB = -1;

		double Distance = MAX_dbl;

		const TArray<PCGExCluster::FNode>& NodesRefA = *ClusterA->Nodes;
		const TArray<PCGExCluster::FNode>& NodesRefB = *ClusterB->Nodes;

		//Brute force find closest points
		for (const PCGExCluster::FNode& Node : NodesRefA)
		{
			FVector NodePos = ClusterA->GetPos(Node);
			const PCGExCluster::FNode& OtherNode = NodesRefB[ClusterB->FindClosestNode(NodePos)];

			if (const double Dist = FVector::DistSquared(NodePos, ClusterB->GetPos(OtherNode));
				Dist < Distance)
			{
				IndexA = Node.PointIndex;
				IndexB = OtherNode.PointIndex;
				Distance = Dist;
			}
		}

		UPCGMetadata* EdgeMetadata = PointIO->GetOut()->Metadata;
		const TSharedRef<PCGExData::FPointIO>& VtxIO = Batch->VtxDataFacade->Source;

		const FPCGMetadataAttribute<int64>* InVtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(VtxIO->GetIn()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		FPCGPoint& EdgePoint = PointIO->GetOut()->GetMutablePoints()[TaskIndex];

		const FPCGPoint& StartPoint = VtxIO->GetOutPoint(IndexA);
		const FPCGPoint& EndPoint = VtxIO->GetOutPoint(IndexB);

		EdgePoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));

		uint32 StartIdx;
		uint32 StartNumEdges;

		uint32 EndIdx;
		uint32 EndNumEdges;

		PCGEx::H64(InVtxEndpointAtt->GetValueFromItemKey(VtxIO->GetInPoint(IndexA).MetadataEntry), StartIdx, StartNumEdges);
		PCGEx::H64(InVtxEndpointAtt->GetValueFromItemKey(VtxIO->GetInPoint(IndexB).MetadataEntry), EndIdx, EndNumEdges);

		FPCGMetadataAttribute<int64>* EdgeEndpointsAtt = static_cast<FPCGMetadataAttribute<int64>*>(EdgeMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeEndpoints));
		FPCGMetadataAttribute<int64>* OutVtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(VtxIO->GetOut()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		EdgeEndpointsAtt->SetValue(EdgePoint.MetadataEntry, PCGEx::H64(StartIdx, EndIdx));
		OutVtxEndpointAtt->SetValue(StartPoint.MetadataEntry, PCGEx::H64(StartIdx, StartNumEdges + 1));
		OutVtxEndpointAtt->SetValue(EndPoint.MetadataEntry, PCGEx::H64(EndIdx, EndNumEdges + 1));

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
