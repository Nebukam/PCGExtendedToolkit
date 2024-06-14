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

PCGEX_INITIALIZE_ELEMENT(BridgeEdgeClusters)

PCGExData::EInit UPCGExBridgeEdgeClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

FPCGExBridgeEdgeClustersContext::~FPCGExBridgeEdgeClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	ProjectionSettings.Cleanup();
}


bool FPCGExBridgeEdgeClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BridgeEdgeClusters)

	PCGEX_FWD(ProjectionSettings)
	PCGEX_FWD(GraphBuilderSettings)

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

		if (!Context->StartProcessingClusters<PCGExBridgeClusters::FProcessorBatch>(
			[](PCGExData::FPointIOTaggedEntries* Entries)
			{
				if (Entries->Entries.Num() == 1)
				{
					// No clusters to consolidate, just dump existing points
					Entries->Entries[0]->InitializeOutput(PCGExData::EInit::DuplicateInput);
					return false;
				}

				return true;
			},
			[&](PCGExBridgeClusters::FProcessorBatch* NewBatch) { return; },
			PCGExGraph::State_ProcessingEdges))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No bridge was created."));
			Context->Done();
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	if (Context->IsState(PCGExGraph::State_ProcessingEdges))
	{
		for (PCGExClusterMT::FClusterProcessorBatchBase* Batch : Context->Batches)
		{
			PCGExBridgeClusters::FProcessorBatch* TBatch = static_cast<PCGExBridgeClusters::FProcessorBatch*>(Batch);
			TBatch->ConnectClusters();
		}

		Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		PCGEX_WAIT_ASYNC

		for (PCGExClusterMT::FClusterProcessorBatchBase* Batch : Context->Batches)
		{
			PCGExBridgeClusters::FProcessorBatch* TBatch = static_cast<PCGExBridgeClusters::FProcessorBatch*>(Batch);
			TBatch->Write();
		}

		Context->Done();
	}

	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecutionComplete();
	}

	return Context->IsDone();
}

namespace PCGExBridgeClusters
{
	FProcessor::FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
		FClusterProcessor(InVtx, InEdges)
	{
	}

	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		PCGEX_SETTINGS(BridgeEdgeClusters)

		return true;
	}

	void FProcessor::ProcessSingleEdge(PCGExGraph::FIndexedEdge& Edge)
	{
		PCGEX_SETTINGS(BridgeEdgeClusters)
	}

	void FProcessor::CompleteWork()
	{
		FClusterProcessor::CompleteWork();
	}

	//////// BATCH

	FProcessorBatch::FProcessorBatch(FPCGContext* InContext, PCGExData::FPointIO* InVtx, const TArrayView<PCGExData::FPointIO*> InEdges):
		TBatch(InContext, InVtx, InEdges)
	{
	}

	FProcessorBatch::~FProcessorBatch()
	{
		PCGEX_DELETE(Merger)

		ConsolidatedEdges = nullptr;
		Bridges.Empty();
	}

	bool FProcessorBatch::PrepareProcessing()
	{
		PCGEX_SETTINGS(BridgeEdgeClusters)
		const FPCGExBridgeEdgeClustersContext* InContext = static_cast<FPCGExBridgeEdgeClustersContext*>(Context);

		ConsolidatedEdges = &EdgeCollection->Emplace_GetRef(PCGExData::EInit::NewOutput);

		if (!TBatch::PrepareProcessing()) { return false; }

		return true;
	}

	bool FProcessorBatch::PrepareSingle(FProcessor* ClusterProcessor)
	{
		PCGEX_SETTINGS(BridgeEdgeClusters)

		ConsolidatedEdges->Tags->Append(ClusterProcessor->EdgesIO->Tags);

		return true;
	}

	void FProcessorBatch::CompleteWork()
	{
		TArray<PCGExData::FPointIO*> ValidEdgeIOs;
		// Gather all valid clusters
		for (const FProcessor* Processor : Processors)
		{
			if (Processor->Cluster->bValid)
			{
				ValidClusters.Add(Processor->Cluster);
				ValidEdgeIOs.Add(Processor->Cluster->EdgesIO);
			}
		}

		if (Processors.Num() != ValidClusters.Num())
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx/edges groups have invalid clusters. Make sure to sanitize the input first."));
		}

		if (ValidClusters.IsEmpty()) { return; } // Skip work completion entirely

		// Fire & forget merge all valid edges
		Merger = new FPCGExPointIOMerger(*ConsolidatedEdges);
		Merger->Append(ValidEdgeIOs);
		Merger->Merge(AsyncManagerPtr);

		////
		PCGEX_SETTINGS(BridgeEdgeClusters)
		const FPCGExBridgeEdgeClustersContext* InContext = static_cast<FPCGExBridgeEdgeClustersContext*>(Context);

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
		for (int i = 0; i < NumBounds; i++) { Bounds[i] = ValidClusters[i]->Bounds; }

		if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay3D)
		{
			PCGExGeo::TDelaunay3* Delaunay = new PCGExGeo::TDelaunay3();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; i++) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, false, nullptr)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Delaunay 3D failed. Are points coplanar? If so, use Delaunay 2D instead.")); }

			Positions.Empty();
			PCGEX_DELETE(Delaunay)
		}
		else if (SafeMethod == EPCGExBridgeClusterMethod::Delaunay2D)
		{
			PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();

			TArray<FVector> Positions;
			Positions.SetNum(NumBounds);

			for (int i = 0; i < NumBounds; i++) { Positions[i] = Bounds[i].GetCenter(); }

			if (Delaunay->Process(Positions, InContext->ProjectionSettings, nullptr)) { Bridges.Append(Delaunay->DelaunayEdges); }
			else { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Delaunay 2D failed.")); }

			Positions.Empty();
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

	void FProcessorBatch::ConnectClusters()
	{
		TArray<FPCGPoint>& MutableEdges = ConsolidatedEdges->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = ConsolidatedEdges->GetOut()->Metadata;

		for (const uint64 Bridge : Bridges)
		{
			FPCGPoint& EdgePoint = MutableEdges.Emplace_GetRef();
			Metadata->InitializeOnSet(EdgePoint.MetadataEntry);

			uint32 Start;
			uint32 End;
			PCGEx::H64(Bridge, Start, End);

			AsyncManagerPtr->Start<FPCGExCreateBridgeTask>(
				MutableEdges.Num() - 1, ConsolidatedEdges,
				this, ValidClusters[Start], ValidClusters[End]);
		}
	}

	void FProcessorBatch::Write() const
	{
		Merger->Write();
	}


	bool FPCGExCreateBridgeTask::ExecuteTask()
	{
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

		UPCGMetadata* EdgeMetadata = PointIO->GetOut()->Metadata;
		FPCGMetadataAttribute<int64>* EdgeEndpointsAtt = static_cast<FPCGMetadataAttribute<int64>*>(EdgeMetadata->GetMutableAttribute(PCGExGraph::Tag_EdgeEndpoints));

		FPCGPoint& EdgePoint = PointIO->GetOut()->GetMutablePoints()[TaskIndex];

		const FPCGPoint& StartPoint = Batch->VtxIO->GetOutPoint(IndexA);
		const FPCGPoint& EndPoint = Batch->VtxIO->GetOutPoint(IndexB);

		EdgePoint.Transform.SetLocation(FMath::Lerp(StartPoint.Transform.GetLocation(), EndPoint.Transform.GetLocation(), 0.5));

		BumpEdgeNum(StartPoint, EndPoint);
		EdgeEndpointsAtt->SetValue(EdgePoint.MetadataEntry, PCGExGraph::HCID(StartPoint.MetadataEntry, EndPoint.MetadataEntry));

		return true;
	}

	void FPCGExCreateBridgeTask::BumpEdgeNum(const FPCGPoint& A, const FPCGPoint& B) const
	{
		FWriteScopeLock WriteScopeLock(Batch->BatchLock);

		FPCGMetadataAttribute<int64>* VtxEndpointAtt = static_cast<FPCGMetadataAttribute<int64>*>(Batch->VtxIO->GetOut()->Metadata->GetMutableAttribute(PCGExGraph::Tag_VtxEndpoint));

		uint32 Idx;
		uint32 Num;
		PCGEx::H64(VtxEndpointAtt->GetValueFromItemKey(A.MetadataEntry), Idx, Num);
		VtxEndpointAtt->SetValue(A.MetadataEntry, PCGEx::H64(Idx, Num + 1));

		PCGEx::H64(VtxEndpointAtt->GetValueFromItemKey(B.MetadataEntry), Idx, Num);
		VtxEndpointAtt->SetValue(B.MetadataEntry, PCGEx::H64(Idx, Num + 1));
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
