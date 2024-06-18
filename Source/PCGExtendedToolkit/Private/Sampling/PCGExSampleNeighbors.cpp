// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNeighbors.h"

#include "Sampling/Neighbors/PCGExNeighborSampleAttribute.h"
#include "Sampling/Neighbors/PCGExNeighborSampleFactoryProvider.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNeighbors"
#define PCGEX_NAMESPACE SampleNeighbors

UPCGExSampleNeighborsSettings::UPCGExSampleNeighborsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

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

	for (UPCGExNeighborSampleOperation* Operation : SamplingOperations)
	{
		Operation->Cleanup();
		PCGEX_DELETE_UOBJECT(Operation)
	}

	SamplingOperations.Empty();
}

bool FPCGExSampleNeighborsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNeighbors)

	TArray<UPCGNeighborSamplerFactoryBase*> SamplerFactories;

	if (!PCGExFactories::GetInputFactories(InContext, PCGExNeighborSample::SourceSamplersLabel, SamplerFactories, {PCGExFactories::EType::Sampler}, false))
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid sampler found."));
		return false;
	}

	// Sort samplers so higher priorities come last, as they have to potential to override values.
	SamplerFactories.Sort([&](const UPCGNeighborSamplerFactoryBase& A, const UPCGNeighborSamplerFactoryBase& B) { return A.Priority < B.Priority; });

	for (const UPCGNeighborSamplerFactoryBase* OperationFactory : SamplerFactories)
	{
		UPCGExNeighborSampleOperation* Operation = OperationFactory->CreateOperation();
		Context->SamplingOperations.Add(Operation);
		Context->RegisterOperation<UPCGExNeighborSampleOperation>(Operation);
	}

	if (Context->SamplingOperations.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any valid samplers."));
		return false;
	}

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
			[&](PCGExClusterMT::TBatch<PCGExSampleNeighbors::FProcessor>* NewBatch) { return; },
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not build any clusters."));
			return true;
		}
	}

	if (!Context->ProcessClusters()) { return false; }


	if (Context->IsDone())
	{
		Context->OutputPointsAndEdges();
		Context->ExecuteEnd();
	}

	return Context->IsDone();
}


namespace PCGExSampleNeighbors
{
	FProcessor::FProcessor(PCGExData::FPointIO* InVtx, PCGExData::FPointIO* InEdges):
		FClusterProcessor(InVtx, InEdges)
	{
	}

	FProcessor::~FProcessor()
	{
		for (UPCGExNeighborSampleOperation* Op : SamplingOperations) { PCGEX_DELETE_UOBJECT(Op) }
		SamplingOperations.Empty();
		VtxOps.Empty();
		EdgeOps.Empty();
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNeighbors)

		if (FClusterProcessor::Process(AsyncManager)) { return false; }

		TArray<UPCGExNeighborSampleOperation*> PointStatePreparation;
		TArray<UPCGExNeighborSampleOperation*> ValueStatePreparation;

		for (const UPCGExNeighborSampleOperation* SamplingOperation : TypedContext->SamplingOperations)
		{
			UPCGExNeighborSampleOperation* OperationCopy = SamplingOperation->CopyOperation<UPCGExNeighborSampleOperation>();

			OperationCopy->PrepareForCluster(Context, Cluster);

			if (!OperationCopy->IsOperationValid())
			{
				PCGEX_DELETE_UOBJECT(OperationCopy)
				continue;
			}

			SamplingOperations.Add(OperationCopy);

			if (OperationCopy->PointState && OperationCopy->PointState->RequiresPerPointPreparation()) { PointStatePreparation.Add(OperationCopy); }
			if (OperationCopy->ValueState && OperationCopy->ValueState->RequiresPerPointPreparation()) { ValueStatePreparation.Add(OperationCopy); }

			if (OperationCopy->BaseSettings.NeighborSource == EPCGExGraphValueSource::Point) { VtxOps.Add(OperationCopy); }
			else { EdgeOps.Add(OperationCopy); }
		}

		for (const PCGExCluster::FNode& Node : Cluster->Nodes)
		{
			for (const UPCGExNeighborSampleOperation* Op : PointStatePreparation) { Op->PointState->PrepareSingle(Node.NodeIndex); }
			for (const UPCGExNeighborSampleOperation* Op : ValueStatePreparation) { Op->ValueState->PrepareSingle(Node.NodeIndex); }
		}

		StartParallelLoopForNodes();

		return true;
	}

	void FProcessor::ProcessSingleNode(PCGExCluster::FNode& Node)
	{
		for (const UPCGExNeighborSampleOperation* Op : VtxOps) { Op->ProcessNodeForPoints(Node.NodeIndex); }
		for (const UPCGExNeighborSampleOperation* Op : EdgeOps) { Op->ProcessNodeForEdges(Node.NodeIndex); }
	}

	void FProcessor::CompleteWork()
	{
		
		for (UPCGExNeighborSampleOperation* Op : SamplingOperations) { Op->FinalizeOperation(); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
