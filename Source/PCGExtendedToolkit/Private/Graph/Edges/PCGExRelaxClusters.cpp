// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/Edges/PCGExRelaxClusters.h"


#include "Graph/Edges/Relaxing/PCGExRelaxClusterOperation.h"

#define LOCTEXT_NAMESPACE "PCGExRelaxClusters"
#define PCGEX_NAMESPACE RelaxClusters

PCGExData::EIOInit UPCGExRelaxClustersSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGExData::EIOInit UPCGExRelaxClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

TArray<FPCGPinProperties> UPCGExRelaxClustersSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExRelaxClusters::SourceOverridesRelaxing)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(RelaxClusters)

bool FPCGExRelaxClustersElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)

	PCGEX_OPERATION_BIND(Relaxing, UPCGExRelaxClusterOperation, PCGExRelaxClusters::SourceOverridesRelaxing)

	return true;
}

bool FPCGExRelaxClustersElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExRelaxClustersElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(RelaxClusters)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExRelaxClusters::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExRelaxClusters::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

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
			InClusterRef, VtxDataFacade->Source, VtxDataFacade->Source, NodeIndexLookup,
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

		for (int i = 0; i < NumNodes; i++) { PBufferRef[i] = SBufferRef[i] = Cluster->GetPos(i); }

		Iterations = Settings->Iterations;

		StartRelaxIteration();
		return true;
	}

	void FProcessor::StartRelaxIteration()
	{
		if (Iterations <= 0) { return; }

		Iterations--;
		std::swap(PrimaryBuffer, SecondaryBuffer);

		RelaxOperation->ReadBuffer = PrimaryBuffer.Get();
		RelaxOperation->WriteBuffer = SecondaryBuffer.Get();

		AsyncManager->FlushTasks(); // TODO Check if this is safe, we need to flush iteration tasks before creating new ones
		
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, IterationGroup)

		IterationGroup->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->StartRelaxIteration();
			};

		PCGEX_SHARED_THIS_DECL
		IterationGroup->StartRanges<FRelaxRangeTask>(
			NumNodes, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize(),
			false, ThisPtr);
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const PCGExMT::FScope& Scope)
	{
		RelaxOperation->ProcessExpandedNode(Node);

		if (!InfluenceDetails.bProgressiveInfluence) { return; }

		(*RelaxOperation->WriteBuffer)[Index] = FMath::Lerp(
			*(RelaxOperation->ReadBuffer->GetData() + Index),
			*(RelaxOperation->WriteBuffer->GetData() + Index),
			InfluenceDetails.GetInfluence(Node.PointIndex));
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
					*(RelaxOperation->WriteBuffer->GetData() + Node.Index),
					InfluenceDetails.GetInfluence(Node.PointIndex));
				MutablePoints[Node.PointIndex].Transform.SetLocation(Position);
			}
		}
		else
		{
			for (const PCGExCluster::FNode& Node : *Cluster->Nodes)
			{
				FVector Position = *(RelaxOperation->WriteBuffer->GetData() + Node.Index);
				MutablePoints[Node.PointIndex].Transform.SetLocation(Position);
			}
		}

		Cluster->WillModifyVtxPositions(true);
		ForwardCluster();
	}

	void FRelaxRangeTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		for (int i = Scope.Start; i < Scope.End; i++) { Processor->ProcessSingleNode(i, *Processor->Cluster->GetNode(i), Scope); }
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
