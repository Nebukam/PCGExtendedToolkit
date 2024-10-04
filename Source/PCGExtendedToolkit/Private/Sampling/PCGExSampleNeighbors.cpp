// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNeighbors.h"


#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNeighbors"
#define PCGEX_NAMESPACE SampleNeighbors

TArray<FPCGPinProperties> UPCGExSampleNeighborsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExNeighborSample::SourceSamplersLabel, "Neighbor samplers.", Required, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNeighborsSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }
PCGExData::EInit UPCGExSampleNeighborsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(SampleNeighbors)

bool FPCGExSampleNeighborsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)

	if (!PCGExFactories::GetInputFactories(InContext, PCGExNeighborSample::SourceSamplersLabel, Context->SamplerFactories, {PCGExFactories::EType::Sampler}, false))
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid sampler found."));
		return false;
	}

	// Sort samplers so higher priorities come last, as they have to potential to override values.
	Context->SamplerFactories.Sort([&](const UPCGExNeighborSamplerFactoryBase& A, const UPCGExNeighborSamplerFactoryBase& B) { return A.Priority < B.Priority; });

	return true;
}

bool FPCGExSampleNeighborsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNeighborsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
				NewBatch->bWriteVtxDataFacade = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGEx::State_Done)

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExSampleNeighbors
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNeighbors::Process);

		if (!FClusterProcessor::Process(InAsyncManager)) { return false; }

		for (const UPCGExNeighborSamplerFactoryBase* OperationFactory : Context->SamplerFactories)
		{
			UPCGExNeighborSampleOperation* SamplingOperation = OperationFactory->CreateOperation(Context);
			SamplingOperation->BindContext(Context);
			SamplingOperation->PrepareForCluster(ExecutionContext, Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);

			if (!SamplingOperation->IsOperationValid()) { continue; }

			SamplingOperations.Add(SamplingOperation);
			if (SamplingOperation->ValueFilters) { OpsWithValueTest.Add(SamplingOperation); }
		}

		ExpandedNodes = Cluster->ExpandedNodes;

		if (!ExpandedNodes)
		{
			ExpandedNodes = Cluster->GetExpandedNodes(false);
			bBuildExpandedNodes = true;
		}

		Cluster->ComputeEdgeLengths();

		StartParallelLoopForRange(NumNodes);

		return true;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 Count)
	{
		if (bBuildExpandedNodes) { (*ExpandedNodes)[Iteration] = PCGExCluster::FExpandedNode(Cluster.Get(), Iteration); }
		for (const UPCGExNeighborSampleOperation* Op : OpsWithValueTest) { Op->ValueFilters->Results[Iteration] = Op->ValueFilters->Test(*(Cluster->Nodes->GetData() + Iteration)); }
	}

	void FProcessor::ProcessSingleNode(const int32 Index, PCGExCluster::FNode& Node, const int32 LoopIdx, const int32 Count)
	{
		for (const UPCGExNeighborSampleOperation* Op : SamplingOperations) { Op->ProcessNode(Index); }
	}

	void FProcessor::CompleteWork()
	{
		StartParallelLoopForNodes();
	}

	void FProcessor::Write()
	{
		for (UPCGExNeighborSampleOperation* Op : SamplingOperations) { Op->FinalizeOperation(); }
		EdgeDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
