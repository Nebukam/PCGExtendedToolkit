// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExConnectClusters.h"

#include "Data/PCGExPointIOMerger.h"






#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExConnectClusters"
#define PCGEX_NAMESPACE ConnectClusters

PCGExData::EInit UPCGExConnectClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }
PCGExData::EInit UPCGExConnectClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(ConnectClusters)


FPCGExConnectClustersContext::~FPCGExConnectClustersContext()
{
	PCGEX_TERMINATE_ASYNC
}


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

bool FPCGExConnectClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExConnectClustersElement::Execute);

	PCGEX_CONTEXT(ConnectClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExBridgeClusters::FProcessorBatch>(
			[&](PCGExData::FPointIOTaggedEntries* Entries)
			{
				if (Entries->Entries.Num() == 1)
				{
					// No clusters to consolidate, just dump existing points
					Context->CurrentIO->InitializeOutput(PCGExData::EInit::Forward);
					Entries->Entries[0]->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}

				return true;
			},
			[&](PCGExBridgeClusters::FProcessorBatch* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No bridge was created."));
			Context->OutputPointsAndEdges();
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	for (PCGExClusterMT::FClusterProcessorBatchBase* Batch : Context->Batches)
	{
		const PCGExBridgeClusters::FProcessorBatch* BridgeBatch = static_cast<PCGExBridgeClusters::FProcessorBatch*>(Batch);
		const int64 ClusterId = BridgeBatch->VtxIO->GetOut()->UID;
		PCGExData::WriteMark(BridgeBatch->ConsolidatedEdges->GetOut()->Metadata, PCGExGraph::Tag_ClusterId, ClusterId);

		FString OutId;
		PCGExGraph::SetClusterVtx(BridgeBatch->VtxIO, OutId);
		PCGExGraph::MarkClusterEdges(BridgeBatch->ConsolidatedEdges, OutId);
	}

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExBridgeClusters
{
	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBridgeClusters::Process);
		PCGEX_SETTINGS(ConnectClusters)

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

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, TSharedPtr<PCGExData::FPointIO> InVtx, const TArrayView<TSharedPtr<PCGExData::FPointIO>> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		ConsolidatedEdges = nullptr;
		Bridges.Empty();
	}

	void FProcessorBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectClusters)
		//const FPCGExConnectClustersContext* InContext = static_cast<FPCGExConnectClustersContext*>(Context);

		ConsolidatedEdges = Context->MainEdges->Emplace_GetRef(PCGExData::EInit::NewOutput);

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

	void FProcessorBatch::Process()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ConnectClusters)

		TBatch<FProcessor>::Process();

		// Start merging right away
		TSet<FName> IgnoreAttributes = {PCGExGraph::Tag_ClusterId};

		Merger = MakeUnique<FPCGExPointIOMerger>(ConsolidatedEdges);
		Merger->Append(Edges);
		Merger->Merge(AsyncManager, &Context->CarryOverDetails);
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		PCGEX_SETTINGS(ConnectClusters)
		ConsolidatedEdges->Tags->Append(ClusterProcessor->EdgesIO->Tags.Get());

		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		const int32 NumValidClusters = GatherValidClusters();

		if (Processors.Num() != NumValidClusters)
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some vtx/edges groups have invalid clusters. Make sure to sanitize the input first."));
		}

		if (ValidClusters.IsEmpty()) { return; } // Skip work completion entirely

		Merger->Write(AsyncManager); // Write base attributes value while finding bridges		

		////
		PCGEX_SETTINGS(ConnectClusters)
		const FPCGExConnectClustersContext* InContext = static_cast<FPCGExConnectClustersContext*>(ExecutionContext);

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
		Bounds.SetNumUninitialized(NumBounds);
		for (int i = 0; i < NumBounds; ++i) { Bounds[i] = ValidClusters[i]->Bounds; }

		if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay3D)
		{
			TUniquePtr<PCGExGeo::TDelaunay3> Delaunay = MakeUnique<PCGExGeo::TDelaunay3>();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; ++i) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, false)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Delaunay 3D failed. Are points coplanar? If so, use Delaunay 2D instead.")); }

			Positions.Empty();
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay2D)
		{
			TUniquePtr<PCGExGeo::TDelaunay2> Delaunay = MakeUnique<PCGExGeo::TDelaunay2>();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; ++i) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, InContext->ProjectionDetails)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Delaunay 2D failed.")); }

			Positions.Empty();
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::LeastEdges)
		{
			TSet<int32> VisitedEdges;
			for (int i = 0; i < NumBounds; ++i)
			{
				VisitedEdges.Add(i); // As to not connect to self or already connected
				double Distance = TNumericLimits<double>::Max();
				int32 ClosestIndex = -1;

				for (int j = 0; j < NumBounds; ++j)
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
			for (int i = 0; i < NumBounds; ++i)
			{
				for (int j = 0; j < NumBounds; ++j)
				{
					if (i == j) { continue; }
					Bridges.Add(PCGEx::H64U(i, j));
				}
			}
		}
	}

	void FProcessorBatch::Write()
	{
		//TArray<FPCGPoint>& MutableEdges = ConsolidatedEdges->GetOut()->GetMutablePoints();
		//UPCGMetadata* Metadata = ConsolidatedEdges->GetOut()->Metadata;

		for (const uint64 Bridge : Bridges)
		{
			int32 EdgePointIndex;
			ConsolidatedEdges->NewPoint(EdgePointIndex);

			uint32 Start;
			uint32 End;
			PCGEx::H64(Bridge, Start, End);

			AsyncManager->Start<FPCGExCreateBridgeTask>(
				EdgePointIndex, ConsolidatedEdges,
				this, ValidClusters[Start], ValidClusters[End]);
		}

		// Force writing cluster ID to Vtx, otherwise we inherit from previous metadata.
		const uint64 ClusterId = VtxIO->GetOut()->UID;
		PCGEx::TAttributeWriter<int64>* ClusterIdWriter = new PCGEx::TAttributeWriter<int64>(PCGExGraph::Tag_ClusterId);
		for (int64& Id : ClusterIdWriter->Values) { Id = ClusterId; }
		PCGExMT::Write(AsyncManager, ClusterIdWriter);

		TBatch<FProcessor>::Write();
		// TODO : OPTIM : We can easily build this batch' cluster by appending existing ones into a big one and just add edges
	}


	bool FPCGExCreateBridgeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		int32 IndexA = -1;
		int32 IndexB = -1;

		double Distance = TNumericLimits<double>::Max();

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

		const FPCGMetadataAttribute<int64>* InVtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(Batch->VtxIO->GetIn()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		FPCGPoint& EdgePoint = PointIO->GetOut()->GetMutablePoints()[TaskIndex];

		const FPCGPoint& StartPoint = Batch->VtxIO->GetOutPoint(IndexA);
		const FPCGPoint& EndPoint = Batch->VtxIO->GetOutPoint(IndexB);

		if (EdgePoint.Transform.IsValid())
		{
			EdgePoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));
		}

		uint32 StartIdx;
		uint32 StartNumEdges;

		uint32 EndIdx;
		uint32 EndNumEdges;

		PCGEx::H64(InVtxEndpointAtt->GetValueFromItemKey(Batch->VtxIO->GetInPoint(IndexA).MetadataEntry), StartIdx, StartNumEdges);
		PCGEx::H64(InVtxEndpointAtt->GetValueFromItemKey(Batch->VtxIO->GetInPoint(IndexB).MetadataEntry), EndIdx, EndNumEdges);

		FPCGMetadataAttribute<int64>* EdgeEndpointsAtt = static_cast<FPCGMetadataAttribute<int64>*>(EdgeMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeEndpoints));
		FPCGMetadataAttribute<int64>* OutVtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(Batch->VtxIO->GetOut()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		if (!EdgeEndpointsAtt || !OutVtxEndpointAtt) { return false; } // Somehow crashes, sometimes? Need to look into it.

		EdgeEndpointsAtt->SetValue(EdgePoint.MetadataEntry, PCGEx::H64(StartIdx, EndIdx));
		OutVtxEndpointAtt->SetValue(StartPoint.MetadataEntry, PCGEx::H64(StartIdx, StartNumEdges + 1));
		OutVtxEndpointAtt->SetValue(EndPoint.MetadataEntry, PCGEx::H64(EndIdx, EndNumEdges + 1));

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
