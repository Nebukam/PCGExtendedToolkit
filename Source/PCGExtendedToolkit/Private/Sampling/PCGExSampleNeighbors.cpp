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

FPCGExSampleNeighborsContext::~FPCGExSampleNeighborsContext()
{
	PCGEX_TERMINATE_ASYNC

	//for (UPCGExNeighborSamplerFactoryBase* Factory : SamplerFactories) { PCGEX_DELETE_UOBJECT(Factory) }
	SamplerFactories.Empty();
}

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

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		if (!Context->StartProcessingClusters<PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>>(
			[](PCGExData::FPointIOTaggedEntries* Entries) { return true; },
			[&](PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>* NewBatch)
			{
				NewBatch->bRequiresWriteStep = true;
				NewBatch->bWriteVtxDataFacade = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }

	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}


namespace PCGExSampleNeighbors
{
	FProcessor::~FProcessor()
	{
		for (UPCGExNeighborSampleOperation* Op : SamplingOperations) { PCGEX_DELETE_OPERATION(Op) }
		if (bBuildExpandedNodes) { PCGEX_DELETE_TARRAY_FULL(ExpandedNodes) }
		SamplingOperations.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNeighbors::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNeighbors)

		if (!FClusterProcessor::Process(AsyncManager)) { return false; }

		for (const UPCGExNeighborSamplerFactoryBase* OperationFactory : TypedContext->SamplerFactories)
		{
			UPCGExNeighborSampleOperation* SamplingOperation = OperationFactory->CreateOperation();
			SamplingOperation->BindContext(TypedContext);
			SamplingOperation->PrepareForCluster(Context, Cluster, VtxDataFacade, EdgeDataFacade);

			if (!SamplingOperation->IsOperationValid())
			{
				PCGEX_DELETE_OPERATION(SamplingOperation)
				continue;
			}

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
		if (bBuildExpandedNodes) { (*ExpandedNodes)[Iteration] = new PCGExCluster::FExpandedNode(Cluster, Iteration); }
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
		EdgeDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
