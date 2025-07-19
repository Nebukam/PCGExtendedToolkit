// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNeighbors.h"


#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNeighbors"
#define PCGEX_NAMESPACE SampleNeighbors

TArray<FPCGPinProperties> UPCGExSampleNeighborsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExNeighborSample::SourceSamplersLabel, "Neighbor samplers.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleNeighborsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExSampleNeighborsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

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
	Context->SamplerFactories.Sort([&](const UPCGExNeighborSamplerFactoryData& A, const UPCGExNeighborSamplerFactoryData& B) { return A.Priority < B.Priority; });

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
		if (!Context->StartProcessingClusters<PCGExSampleNeighbors::FBatch>(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExSampleNeighbors::FBatch>& NewBatch)
			{
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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNeighbors::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		for (const UPCGExNeighborSamplerFactoryData* OperationFactory : Context->SamplerFactories)
		{
			TSharedPtr<FPCGExNeighborSampleOperation> SamplingOperation = OperationFactory->CreateOperation(Context);
			SamplingOperation->BindContext(Context);
			SamplingOperation->PrepareForCluster(ExecutionContext, Cluster.ToSharedRef(), VtxDataFacade, EdgeDataFacade);

			if (!SamplingOperation->IsOperationValid()) { continue; }

			SamplingOperations.Add(SamplingOperation);
			if (SamplingOperation->ValueFilters) { OpsWithValueTest.Add(SamplingOperation); }
		}

		Cluster->ComputeEdgeLengths();

		if (!OpsWithValueTest.IsEmpty()) { StartParallelLoopForRange(NumNodes); }
		else { StartParallelLoopForNodes(); }


		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			for (const TSharedPtr<FPCGExNeighborSampleOperation>& Op : OpsWithValueTest)
			{
				Op->ValueFilters->Results[Index] = Op->ValueFilters->Test(*Cluster->GetNode(Index));
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		StartParallelLoopForNodes();
	}

	void FProcessor::PrepareLoopScopesForNodes(const TArray<PCGExMT::FScope>& Loops)
	{
		for (const TSharedPtr<FPCGExNeighborSampleOperation>& Op : SamplingOperations) { Op->PrepareForLoops(Loops); }
	}

	void FProcessor::ProcessNodes(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			for (const TSharedPtr<FPCGExNeighborSampleOperation>& Op : SamplingOperations) { Op->ProcessNode(Index, Scope); }
		}
	}

	void FProcessor::Write()
	{
		for (const TSharedPtr<FPCGExNeighborSampleOperation>& Op : SamplingOperations) { Op->CompleteOperation(); }
		EdgeDataFacade->WriteFastest(AsyncManager);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExSampleNeighborsContext, UPCGExSampleNeighborsSettings>::Cleanup();
		SamplingOperations.Empty();
		OpsWithValueTest.Empty();
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNeighbors)
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);

		for (const UPCGExNeighborSamplerFactoryData* Factory : Context->SamplerFactories)
		{
			Factory->RegisterVtxBuffersDependencies(Context, VtxDataFacade, FacadePreloader);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
