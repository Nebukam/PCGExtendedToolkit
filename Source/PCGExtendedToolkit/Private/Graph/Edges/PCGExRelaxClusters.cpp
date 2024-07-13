// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRelaxClusters.h"

#include "Graph/Edges/Relaxing/PCGExRelaxClusterOperation.h"
#include "Graph/Edges/Relaxing/PCGExForceDirectedRelax.h"

#define LOCTEXT_NAMESPACE "PCGExRelaxClusters"
#define PCGEX_NAMESPACE RelaxClusters

PCGExData::EInit UPCGExRelaxClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGExData::EInit UPCGExRelaxClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(RelaxClusters)

FPCGExRelaxClustersContext::~FPCGExRelaxClustersContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExRelaxClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)

	PCGEX_OPERATION_BIND(Relaxing, UPCGExForceDirectedRelax)

	return true;
}

bool FPCGExRelaxClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelaxClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExRelaxClusters::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExRelaxClusters::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExRelaxClusters
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(PrimaryBuffer)
		PCGEX_DELETE(SecondaryBuffer)

		PCGEX_DELETE_UOBJECT(RelaxOperation)

		//if (bBuildExpandedNodes) { PCGEX_DELETE_TARRAY_FULL(ExpandedNodes) } // Keep those cached since we forward expanded cluster
	}

	PCGExCluster::FCluster* FProcessor::HandleCachedCluster(const PCGExCluster::FCluster* InClusterRef)
	{
		bDeleteCluster = false;
		return new PCGExCluster::FCluster(InClusterRef, VtxIO, EdgesIO, true, false, false);
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRelaxClusters::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RelaxClusters)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		InfluenceDetails = Settings->InfluenceDetails;
		if (!InfluenceDetails.Init(Context, VtxDataFacade)) { return false; }

		RelaxOperation = TypedContext->Relaxing->CopyOperation<UPCGExRelaxClusterOperation>();
		RelaxOperation->PrepareForCluster(Cluster);

		PrimaryBuffer = new TArray<FVector>();
		SecondaryBuffer = new TArray<FVector>();

		PCGEX_SET_NUM_UNINITIALIZED_PTR(PrimaryBuffer, NumNodes)
		PCGEX_SET_NUM_UNINITIALIZED_PTR(SecondaryBuffer, NumNodes)

		TArray<FVector>& PBufferRef = (*PrimaryBuffer);
		TArray<FVector>& SBufferRef = (*SecondaryBuffer);

		for (int i = 0; i < NumNodes; i++) { PBufferRef[i] = SBufferRef[i] = (Cluster->Nodes->GetData() + i)->Position; }

		ExpandedNodes = Cluster->ExpandedNodes;
		Iterations = Settings->Iterations;

		IterationGroup = AsyncManagerPtr->CreateGroup();

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
			StartParallelLoopForRange(NumNodes);
		}
		else
		{
			StartRelaxIteration();
		}

		return true;
	}

	void FProcessor::StartRelaxIteration()
	{
		if (Iterations <= 0) { return; }

		Iterations--;
		std::swap(PrimaryBuffer, SecondaryBuffer);

		RelaxOperation->ReadBuffer = PrimaryBuffer;
		RelaxOperation->WriteBuffer = SecondaryBuffer;

		IterationGroup->SetOnCompleteCallback([&]() { StartRelaxIteration(); });
		IterationGroup->StartRanges<FRelaxRangeTask>(
			NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchIteration(),
			nullptr, this);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration)
	{
		(*ExpandedNodes)[Iteration] = new PCGExCluster::FExpandedNode(Cluster, Iteration);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node)
	{
		RelaxOperation->ProcessExpandedNode(*(ExpandedNodes->GetData() + Index));

		if (!InfluenceDetails.bProgressiveInfluence) { return; }

		(*RelaxOperation->WriteBuffer)[Index] = FMath::Lerp(
			*(RelaxOperation->ReadBuffer->GetData() + Index),
			*(RelaxOperation->WriteBuffer->GetData() + Index),
			InfluenceDetails.GetInfluence(Node.PointIndex));
	}

	void FProcessor::CompleteWork()
	{
		if (!bBuildExpandedNodes) { return; }
		StartRelaxIteration();
	}

	void FProcessor::Write()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(RelaxClusters)

		FClusterProcessor::Write();

		TArray<FPCGPoint>& MutablePoints = VtxIO->GetOut()->GetMutablePoints();
		if (!InfluenceDetails.bProgressiveInfluence)
		{
			const TArray<FPCGPoint>& OriginalPoints = VtxIO->GetIn()->GetPoints();
			for (PCGExCluster::FNode& Node : *Cluster->Nodes)
			{
				FVector Position = FMath::Lerp(
					OriginalPoints[Node.PointIndex].Transform.GetLocation(),
					*(RelaxOperation->WriteBuffer->GetData() + Node.NodeIndex),
					InfluenceDetails.GetInfluence(Node.PointIndex));
				MutablePoints[Node.PointIndex].Transform.SetLocation(Position);
				Node.Position = Position;
			}
		}
		else
		{
			for (PCGExCluster::FNode& Node : *Cluster->Nodes)
			{
				FVector Position = *(RelaxOperation->WriteBuffer->GetData() + Node.NodeIndex);
				MutablePoints[Node.PointIndex].Transform.SetLocation(Position);
				Node.Position = Position;
			}
		}

		Cluster->WillModifyVtxPositions(true);
		ForwardCluster(true);
	}

	bool FRelaxRangeTask::ExecuteTask()
	{
		const int32 StartIndex = PCGEx::H64A(Scope);
		const int32 NumIterations = PCGEx::H64B(Scope);
		
		for (int i = 0; i < NumIterations; i++)
		{
			const int32 Index = StartIndex + i;
			Processor->ProcessSingleNode(Index, *(Processor->Cluster->Nodes->GetData() + Index));
		}

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
