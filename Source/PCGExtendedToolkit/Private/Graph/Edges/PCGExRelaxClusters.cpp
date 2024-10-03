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

bool FPCGExRelaxClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)

	PCGEX_OPERATION_BIND(Relaxing, UPCGExRelaxClusterOperation)

	return true;
}

bool FPCGExRelaxClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelaxClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)
	PCGEX_EXECUTION_CHECK

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExRelaxClusters::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExRelaxClusters::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters(PCGExMT::State_Done)) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExRelaxClusters
{
	FProcessor::~FProcessor()
	{
	}

	TSharedPtr<PCGExCluster::FCluster> FProcessor::HandleCachedCluster(const TSharedRef<PCGExCluster::FCluster>& InClusterRef)
	{
		return MakeShared<PCGExCluster::FCluster>(
			InClusterRef, VtxDataFacade->Source, VtxDataFacade->Source,
			true, false, false);
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExRelaxClusters::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		InfluenceDetails = Settings->InfluenceDetails;
		if (!InfluenceDetails.Init(ExecutionContext, VtxDataFacade)) { return false; }

		RelaxOperation = Context->Relaxing->CopyOperation<UPCGExRelaxClusterOperation>();
		RelaxOperation->PrepareForCluster(Cluster.Get());

		PrimaryBuffer = MakeShared<TArray<FVector>>();
		SecondaryBuffer = MakeShared<TArray<FVector>>();

		PrimaryBuffer->SetNumUninitialized(NumNodes);
		SecondaryBuffer->SetNumUninitialized(NumNodes);

		TArray<FVector>& PBufferRef = (*PrimaryBuffer);
		TArray<FVector>& SBufferRef = (*SecondaryBuffer);

		for (int i = 0; i < NumNodes; ++i) { PBufferRef[i] = SBufferRef[i] = Cluster->GetPos(i); }

		ExpandedNodes = Cluster->ExpandedNodes;
		Iterations = Settings->Iterations;

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

		RelaxOperation->ReadBuffer = PrimaryBuffer.Get();
		RelaxOperation->WriteBuffer = SecondaryBuffer.Get();

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, IterationGroup)
		IterationGroup->OnCompleteCallback = [&]() { StartRelaxIteration(); };
		IterationGroup->StartRanges<FRelaxRangeTask>(
			NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
			nullptr, this);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		(*ExpandedNodes)[Iteration] = PCGExCluster::FExpandedNode(Cluster.Get(), Iteration);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
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
		FClusterProcessor::Write();

		TArray<FPCGPoint>& MutablePoints = VtxDataFacade->GetOut()->GetMutablePoints();
		if (!InfluenceDetails.bProgressiveInfluence)
		{
			const TArray<FPCGPoint>& OriginalPoints = VtxDataFacade->GetIn()->GetPoints();
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes)
			{
				FVector Position = FMath::Lerp(
					OriginalPoints[Node.PointIndex].Transform.GetLocation(),
					*(RelaxOperation->WriteBuffer->GetData() + Node.NodeIndex),
					InfluenceDetails.GetInfluence(Node.PointIndex));
				MutablePoints[Node.PointIndex].Transform.SetLocation(Position);
			}
		}
		else
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes)
			{
				FVector Position = *(RelaxOperation->WriteBuffer->GetData() + Node.NodeIndex);
				MutablePoints[Node.PointIndex].Transform.SetLocation(Position);
			}
		}

		Cluster->WillModifyVtxPositions(true);
		ForwardCluster();
	}

	bool FRelaxRangeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const int32 StartIndex = PCGEx::H64A(Scope);
		const int32 NumIterations = PCGEx::H64B(Scope);

		for (int i = 0; i < NumIterations; ++i)
		{
			const int32 Index = StartIndex + i;
			Processor->ProcessSingleNode(Index, *(Processor->Cluster->Nodes->GetData() + Index), TaskIndex, NumIterations);
		}

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
